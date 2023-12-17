//
// Control code for the OpenIFS application in the climateprediction.net project
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) November 2022
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include "CPDN_control_code.h"

int main(int argc, char** argv) {
    std::string ifsdata_file, ic_ancil_file, climate_data_file, horiz_resolution, vert_resolution, grid_type;
    std::string project_path, tmpstr1, tmpstr2, tmpstr3, tmpstr4, tmpstr5;
    std::string ifs_line="", iter="0", ifs_word="", second_part, first_part, upload_file_name, last_line="";
    std::string resolved_name, upload_file, result_base_name;
    std::string wu_name="", project_dir="", version="", strCmd="";
    int upload_interval, trickle_upload_frequency, timestep_interval, ICM_file_interval, retval=0, i, j;
    int process_status=1, restart_interval, current_iter=0, count=0, trickle_upload_count, skip_main_loop=0;
    int last_cpu_time, restart_cpu_time = 0, upload_file_number, model_completed, restart_iter, standalone=0;
    int last_upload; // The time of the last upload file (in seconds)
    std::string last_iter = "0";
    char *pathvar=NULL;
    long handleProcess;
    double tv_sec, tv_usec, fraction_done, current_cpu_time=0, total_nsteps = 0;
    struct dirent *dir;
    struct rusage usage;
    regex_t regex;
    DIR *dirp=NULL;
    ZipFileList zfl;
    std::ifstream ifs_stat_file;


    // Set defaults for input arguments
    std::string OIFS_EXPID;           // model experiment id, must match string in filenames
    std::string namelist="fort.4";    // namelist file, this name is fixed

    // Initialise BOINC
    retval = initialise_boinc(wu_name, project_dir, version, standalone);
    if (retval) {
       cerr << "..BOINC initialisation failed" << "\n";
       return retval;
    }


    cerr << "wu_name: " << wu_name << '\n';
    cerr << "project_dir: " << project_dir << '\n';
    cerr << "version: " << version << '\n';

    cerr << "(argv0) " << argv[0] << '\n';
    cerr << "(argv1) start_date: " << argv[1] << '\n';
    cerr << "(argv2) exptid: " << argv[2] << '\n';
    cerr << "(argv3) unique_member_id: " << argv[3] << '\n';
    cerr << "(argv4) batchid: " << argv[4] << '\n';
    cerr << "(argv5) wuid: " << argv[5] << '\n';
    cerr << "(argv6) fclen: " << argv[6] << '\n';
    cerr << "(argv7) app_name: " << argv[7] << '\n';
    cerr << "(argv8) nthreads: " << argv[8] << std::endl;

    // Read the exptid, umid, batchid, wuid, fclen, app_name, number of threads from the command line
    std::string start_date = argv[1]; // simulation start date
    std::string exptid = argv[2];     // OpenIFS experiment id
    std::string unique_member_id = argv[3];  // umid
    std::string batchid = argv[4];    // batch id
    std::string wuid = argv[5];       // workunit id
    std::string fclen = argv[6];      // number of simulation days
    std::string app_name = argv[7];   // CPDN app name
    std::string nthreads = argv[8];   // number of OPENMP threads

    OIFS_EXPID = exptid;

    double num_days = atof(fclen.c_str()); // number of simulation days
    int num_days_trunc = (int) num_days; // number of simulation days truncated to an integer

    // Get the slots path (the current working path)
    std::string slot_path = std::filesystem::current_path();
    if (slot_path.empty()) {
      cerr << "..current_path() returned empty" << std::endl;
    }
    else {
      cerr << "Working directory is: "<< slot_path << '\n';      
    }

    if (!standalone) {

      // Get the project path
      project_path = project_dir + std::string("/");
      cerr << "Project directory is: " << project_path << '\n';

      // Get the app version and re-parse to add a dot
      if (version.length()==2) {
         version = version.insert(0,".");
         //cerr << "version: " << version << '\n';
      }
      else if (version.length()==3) {
         version = version.insert(1,".");
         //cerr << "version: " << version << '\n';
      }
      else if (version.length()==4) {
         version = version.insert(2,".");
         //cerr << "version: " << version << '\n';
      }
      else {
         cerr << "..Error with the length of app_version, length is: " << version.length() << '\n';
         return 1;
      }

      cerr << "app name: " << app_name << '\n';
      cerr << "version: " << version << '\n';
    }
    // Running in standalone
    else {
      cerr << "Running in standalone mode" << '\n';
      // Set the project path
      project_path = slot_path + std::string("/../projects/");
      cerr << "Project directory is: " << project_path << '\n';

      // In standalone get the app version from the command line
      version = argv[9];
      cerr << "app name: " << app_name << '\n'; 
      cerr << "(argv9) app_version: " << argv[9] << '\n'; 

      // In standalone get whether to skip the main loop from the command line
      skip_main_loop = atoi(argv[10]);
      cerr << "skip main loop: " << skip_main_loop << '\n'; 
      cerr << "(argv10) skip_main_loop: " << atoi(argv[10]) << '\n'; 
    }

    call_boinc_begin_critical_section();

    // Create temporary folder for moving the results to and uploading the results from
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string temp_path = project_path + app_name + std::string("_") + wuid;
    cerr << "Location of temp folder: " << temp_path << '\n';
    if ( !file_exists(temp_path) ) {
      if (mkdir(temp_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) cerr << "..mkdir for temp folder for results failed" << std::endl;
    }

    // Move and unzip app file
    retval = move_and_unzip_app_file(app_name, version, project_path, slot_path);
    if (retval) {
      cerr << "..move_and_unzip_app_file failed" << "\n";
      return retval;
    }


    //------------------------------------------Process the namelist-----------------------------------------
    std::string namelist_zip = slot_path + std::string("/") + app_name + std::string("_") + unique_member_id + std::string("_") + start_date +\
                      std::string("_") + std::to_string(num_days_trunc) + std::string("_") + batchid + std::string("_") + wuid + std::string(".zip");

    // Get the name of the 'jf_' filename from a link within the namelist file
    std::string wu_source = get_tag(namelist_zip);

    // Copy the namelist files to the working directory
    std::string wu_destination = namelist_zip;
    cerr << "Copying the namelist files from: " << wu_source << " to: " << wu_destination << '\n';
    retval = call_boinc_copy(wu_source, wu_destination);
    if (retval) {
       cerr << "..Copying the namelist files to the working directory failed" << std::endl;
       return retval;
    }

    // Unzip the namelist zip file
    cerr << "Unzipping the namelist zip file: " << namelist_zip << '\n';
    retval = call_boinc_unzip(namelist_zip, slot_path);
    if (retval) {
       cerr << "..Unzipping the namelist file failed" << std::endl;
       return retval;
    }
    // Remove the zip file
    else {
       std::remove(namelist_zip.c_str());
    }


    // Parse the fort.4 namelist for the filenames and variables
    std::string namelist_file = slot_path + std::string("/") + namelist;
    std::string namelist_line="", delimiter="=";
    std::ifstream namelist_filestream;

    // Check for the existence of the namelist
    if( !file_exists(namelist_file) ) {
       cerr << "..The namelist file does not exist: " << namelist_file << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Open the namelist file
    if(!(namelist_filestream.is_open())) {
       namelist_filestream.open(namelist_file);
    }

    // Read the namelist file
    while(std::getline(namelist_filestream, namelist_line)) { //get 1 row as a string
       std::istringstream nss(namelist_line);   //put line into stringstream

       if (nss.str().find("IFSDATA_FILE") != std::string::npos) {
          ifsdata_file = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          ifsdata_file.erase(std::remove(ifsdata_file.begin(), ifsdata_file.end(), ' '), ifsdata_file.end());
          cerr << "ifsdata_file: " << ifsdata_file << '\n';
       }
       else if (nss.str().find("IC_ANCIL_FILE") != std::string::npos) {
          ic_ancil_file = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          ic_ancil_file.erase(std::remove(ic_ancil_file.begin(), ic_ancil_file.end(), ' '), ic_ancil_file.end());
          cerr << "ic_ancil_file: " << ic_ancil_file << '\n'; 
       }
       else if (nss.str().find("CLIMATE_DATA_FILE") != std::string::npos) {
          climate_data_file = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          climate_data_file.erase(std::remove(climate_data_file.begin(),climate_data_file.end(),' '), climate_data_file.end());
          cerr << "climate_data_file: " << climate_data_file << '\n';
       }
       else if (nss.str().find("HORIZ_RESOLUTION") != std::string::npos) {
          horiz_resolution = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          horiz_resolution.erase(std::remove(horiz_resolution.begin(),horiz_resolution.end(),' '), horiz_resolution.end());
          cerr << "horiz_resolution: " << horiz_resolution << '\n';
       }
       else if (nss.str().find("VERT_RESOLUTION") != std::string::npos) {
          vert_resolution = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          vert_resolution.erase(std::remove(vert_resolution.begin(), vert_resolution.end(), ' '), vert_resolution.end());
          cerr << "vert_resolution: " << vert_resolution << '\n';
       }
       else if (nss.str().find("GRID_TYPE") != std::string::npos) {
          grid_type = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          grid_type.erase(std::remove(grid_type.begin(), grid_type.end(),' '), grid_type.end());
          cerr << "grid_type: " << grid_type << '\n';
       }
       else if (nss.str().find("UPLOAD_INTERVAL") != std::string::npos) {
          tmpstr1 = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          tmpstr1.erase(std::remove(tmpstr1.begin(), tmpstr1.end(),' '), tmpstr1.end());
          upload_interval=std::stoi(tmpstr1);
          cerr << "upload_interval: " << upload_interval << '\n';
       }
       else if (nss.str().find("TRICKLE_UPLOAD_FREQUENCY") != std::string::npos) {
          tmpstr2 = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
          tmpstr2.erase(std::remove(tmpstr2.begin(), tmpstr2.end(),' '), tmpstr2.end());
          trickle_upload_frequency=std::stoi(tmpstr2);
          cerr << "trickle_upload_frequency: " << trickle_upload_frequency << '\n';
       }
       else if (nss.str().find("UTSTEP") != std::string::npos) {
          tmpstr3 = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace
	  tmpstr3.erase(std::remove(tmpstr3.begin(), tmpstr3.end(),','), tmpstr3.end());
          tmpstr3.erase(std::remove(tmpstr3.begin(), tmpstr3.end(),' '), tmpstr3.end());
          timestep_interval = std::stoi(tmpstr3);
          cerr << "utstep: " << timestep_interval << '\n';
       }
       else if (nss.str().find("!NFRPOS") != std::string::npos) {
          tmpstr4 = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace and commas
          tmpstr4.erase(std::remove(tmpstr4.begin(), tmpstr4.end(),','), tmpstr4.end());
          tmpstr4.erase(std::remove(tmpstr4.begin(), tmpstr4.end(),' '), tmpstr4.end());
          ICM_file_interval = std::stoi(tmpstr4);
          cerr << "nfrpos: " << ICM_file_interval << '\n';
       }
       else if (nss.str().find("NFRRES") != std::string::npos) {     // frequency of model output: +ve steps, -ve in hours.
          tmpstr5 = nss.str().substr(nss.str().find(delimiter)+1, nss.str().length()-1);
          // Remove any whitespace and commas
          tmpstr5.erase(std::remove(tmpstr5.begin(), tmpstr5.end(),','), tmpstr5.end());
          tmpstr5.erase(std::remove(tmpstr5.begin(), tmpstr5.end(),' '), tmpstr5.end());
          if ( check_stoi(tmpstr5) ) {
            restart_interval = stoi(tmpstr5);
          } else {
            cerr << "..Warning, unable to read restart interval, setting to zero, got string: " << tmpstr5 << std::endl;
            restart_interval = 0;
          }
       }
    }
    namelist_filestream.close();

    //-------------------------------------------------------------------------------------------------------


    // restart frequency might be in units of hrs, convert to model steps
    if ( restart_interval < 0 )   restart_interval = abs(restart_interval)*3600 / timestep_interval;
    cerr << "nfrres: restart dump frequency (steps) " << restart_interval << '\n';

    // this should match CUSTEP in fort.4. If it doesn't we have a problem
    total_nsteps = (num_days * 86400.0) / (double) timestep_interval;


    // Process the ic_ancil_file:
    std::string ic_ancil_zip = slot_path + std::string("/") + ic_ancil_file + std::string(".zip");

    // For transfer downloading, BOINC renames download files to jf_HEXADECIMAL-NUMBER, these files
    // need to be renamed back to the original name
    // Get the name of the 'jf_' filename from a link within the ic_ancil_file
    std::string ic_ancil_source = get_tag(ic_ancil_zip);

    // Copy the IC ancils to working directory
    std::string ic_ancil_destination = ic_ancil_zip;
    cerr << "Copying IC ancils from: " << ic_ancil_source << " to: " << ic_ancil_destination << '\n';
    retval = call_boinc_copy(ic_ancil_source, ic_ancil_destination);
    if (retval) {
       cerr << "..Copying the IC ancils to the working directory failed" << std::endl;
       return retval;
    }

    // Unzip the IC ancils zip file
    cerr << "Unzipping the IC ancils zip file: " << ic_ancil_zip << '\n';
    retval = call_boinc_unzip(ic_ancil_zip, slot_path);
    if (retval) {
       cerr << "..Unzipping the IC ancils file failed" << std::endl;
       return retval;
    }
    // Remove the zip file
    else {
       std::remove(ic_ancil_zip.c_str());
    }


    // Process the ifsdata_file:
    // Make the ifsdata directory
    std::string ifsdata_folder = slot_path + std::string("/ifsdata");
    // Check if ifsdata folder does not already exists or is empty
    if ( !file_exists(ifsdata_folder) ) {
      if (mkdir(ifsdata_folder.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) cerr << "..mkdir for ifsdata folder failed" << '\n';

      // Get the name of the 'jf_' filename from a link within the ifsdata_file
      std::string ifsdata_source = get_tag(slot_path + std::string("/") + ifsdata_file + std::string(".zip"));

      // Copy the ifsdata_file to the working directory
      std::string ifsdata_destination = ifsdata_folder + std::string("/") + ifsdata_file + std::string(".zip");
      cerr << "Copying the ifsdata_file from: " << ifsdata_source << " to: " << ifsdata_destination << '\n';
      retval = call_boinc_copy(ifsdata_source, ifsdata_destination);
      if (retval) {
         cerr << "..Copying the ifsdata file to the working directory failed" << std::endl;
         return retval;
      }

      // Unzip the ifsdata_file zip file
      std::string ifsdata_zip = ifsdata_folder + std::string("/") + ifsdata_file + std::string(".zip");
      cerr << "Unzipping the ifsdata_zip file: " << ifsdata_zip << '\n';
      retval = call_boinc_unzip(ifsdata_zip, ifsdata_folder + std::string("/"));
      if (retval) {
         cerr << "..Unzipping the ifsdata_zip file failed" << std::endl;
         return retval;
      }
      // Remove the zip file
      else {
         std::remove(ifsdata_zip.c_str());
      }
    }

    // Process the climate_data_file:
    // Make the climate data directory
    std::string climate_data_path = slot_path + std::string("/") + horiz_resolution + grid_type;
    // Check if climate_data folder does not already exists or is empty
    if ( !file_exists(climate_data_path) ) {
      if (mkdir(climate_data_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) \
                       cerr << "..mkdir for the climate data folder failed" << std::endl;

      // Get the name of the 'jf_' filename from a link within the climate_data_file
      std::string climate_data_source = get_tag(slot_path + std::string("/") + climate_data_file + std::string(".zip"));

      // Copy the climate data file to working directory
      std::string climate_data_destination = climate_data_path + std::string("/") + climate_data_file + std::string(".zip");
      cerr << "Copying the climate data file from: " << climate_data_source << " to: " << climate_data_destination << '\n';
      retval = call_boinc_copy(climate_data_source, climate_data_destination);
      if (retval) {
         cerr << "..Copying the climate data file to the working directory failed" << std::endl;
         return retval;
      }	

      // Unzip the climate data zip file
      std::string climate_zip = climate_data_destination;
      cerr << "Unzipping the climate data zip file: " << climate_zip << '\n';
      retval = call_boinc_unzip(climate_zip, climate_data_path);
      if (retval) {
         cerr << "..Unzipping the climate data file failed" << std::endl;
         return retval;
      }
      // Remove the zip file
      else {
         std::remove(climate_zip.c_str());
      }
    }


    //------------------------------------Set the environmental variables------------------------------------

    // Set the OIFS_DUMMY_ACTION environmental variable, this controls what OpenIFS does if it goes into a dummy subroutine
    // Possible values are: 'quiet', 'verbose' or 'abort'
    std::string OIFS_var("OIFS_DUMMY_ACTION=abort");
    if (putenv((char *)OIFS_var.c_str())) {
      cerr << "..Setting the OIFS_DUMMY_ACTION environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("OIFS_DUMMY_ACTION");
    //cerr << "The OIFS_DUMMY_ACTION environmental variable is: " << pathvar << '\n';

    // Set the OMP_NUM_THREADS environmental variable, the number of threads
    std::string OMP_NUM_var = std::string("OMP_NUM_THREADS=") + nthreads;
    if (putenv((char *)OMP_NUM_var.c_str())) {
      cerr << "..Setting the OMP_NUM_THREADS environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("OMP_NUM_THREADS");
    //cerr << "The OMP_NUM_THREADS environmental variable is: " << pathvar << '\n';

    // Set the OMP_SCHEDULE environmental variable, this enforces static thread scheduling
    std::string OMP_SCHED_var("OMP_SCHEDULE=STATIC");
    if (putenv((char *)OMP_SCHED_var.c_str())) {
      cerr << "..Setting the OMP_SCHEDULE environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("OMP_SCHEDULE");
    //cerr << "The OMP_SCHEDULE environmental variable is: " << pathvar << '\n';

    // Set the DR_HOOK environmental variable, this controls the tracing facility in OpenIFS, off=0 and on=1
    std::string DR_HOOK_var("DR_HOOK=1");
    if (putenv((char *)DR_HOOK_var.c_str())) {
      cerr << "..Setting the DR_HOOK environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("DR_HOOK");
    //cerr << "The DR_HOOK environmental variable is: " << pathvar << '\n';

    // Set the DR_HOOK_HEAPCHECK environmental variable, this ensures the heap size statistics are reported
    std::string DR_HOOK_HEAP_var("DR_HOOK_HEAPCHECK=no");
    if (putenv((char *)DR_HOOK_HEAP_var.c_str())) {
      cerr << "..Setting the DR_HOOK_HEAPCHECK environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("DR_HOOK_HEAPCHECK");
    //cerr << "The DR_HOOK_HEAPCHECK environmental variable is: " << pathvar << '\n';

    // Set the DR_HOOK_STACKCHECK environmental variable, this ensures the stack size statistics are reported
    std::string DR_HOOK_STACK_var("DR_HOOK_STACKCHECK=no");
    if (putenv((char *)DR_HOOK_STACK_var.c_str())) {
      cerr << "..Setting the DR_HOOK_STACKCHECK environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("DR_HOOK_STACKCHECK");
    //cerr << "The DR_HOOK_STACKCHECK environmental variable is: " << pathvar << '\n';

    // Set the EC_MEMINFO environment variable, only applies to OpenIFS 43r3.
    // Disable EC_MEMINFO to remove the useless EC_MEMINFO messages to the stdout file to reduce filesize.
    std::string EC_MEMINFO("EC_MEMINFO=0");
    if (putenv((char *)EC_MEMINFO.c_str())) {
       cerr << "..Setting the EC_MEMINFO environment variable failed" << std::endl;
       return 1;
    }
    pathvar = getenv("EC_MEMINFO");
    //cerr << "The EC_MEMINFO environment variable is: " << pathvar << '\n';

    // Disable Heap memory stats at end of run; does not work for CPDN version of OpenIFS
    std::string EC_PROFILE_HEAP("EC_PROFILE_HEAP=0");
    if (putenv((char *)EC_PROFILE_HEAP.c_str())) {
       cerr << "..Setting the EC_PROFILE_HEAP environment variable failed" << std::endl;
       return 1;
    }
    pathvar = getenv("EC_PROFILE_HEAP");
    //cerr << "The EC_PROFILE_HEAP environment variable is: " << pathvar << '\n';

    // Disable all memory stats at end of run; does not work for CPDN version of OpenIFS
    std::string EC_PROFILE_MEM("EC_PROFILE_MEM=0");
    if (putenv((char *)EC_PROFILE_MEM.c_str())) {
       cerr << "..Setting the EC_PROFILE_MEM environment variable failed" << std::endl;
       return 1;
    }
    pathvar = getenv("EC_PROFILE_MEM");
    //cerr << "The EC_PROFILE_MEM environment variable is: " << pathvar << '\n';

    // Set the OMP_STACKSIZE environmental variable, OpenIFS needs more stack memory per process
    std::string OMP_STACK_var("OMP_STACKSIZE=128M");
    if (putenv((char *)OMP_STACK_var.c_str())) {
      cerr << "..Setting the OMP_STACKSIZE environmental variable failed" << std::endl;
      return 1;
    }
    pathvar = getenv("OMP_STACKSIZE");
    //cerr << "The OMP_STACKSIZE environmental variable is: " << pathvar << '\n';

    //-------------------------------------------------------------------------------------------------------

    // Set the core dump size to 0
    struct rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &core_limits) != 0) {
       cerr << "..Setting the core dump size to 0 failed" << std::endl;
       return 1;
    }

    // Set the stack limit to be unlimited
    struct rlimit stack_limits;
    // In macOS we cannot set the stack size limit to infinity
    #ifndef __APPLE__ // Linux
       stack_limits.rlim_cur = stack_limits.rlim_max = RLIM_INFINITY;
       if (setrlimit(RLIMIT_STACK, &stack_limits) != 0) {
          cerr << "..Setting the stack limit to unlimited failed" << std::endl;
          return 1;
       }
    #endif


    // Define the name and location of the progress file and the rcf file
    std::string progress_file = slot_path + std::string("/progress_file_") + wuid + std::string(".xml");
    std::string rcf_file = slot_path + std::string("/rcf");

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    cerr << "Checking for rcf file and progress XML file: " << progress_file << '\n';

    // Handle the cases of the various states of the rcf file and progress file
    if ( !file_exists(progress_file) && !file_exists(rcf_file) ) {
       // If both progress file and rcf file do not exist, then model has not run, then set the initial values of run
       last_cpu_time = 0;
       upload_file_number = 0;
       last_iter = "0";
       last_upload = 0;
       model_completed = 0;
    } else if ( file_exists(progress_file) && file_is_empty(progress_file) ) {
       // If progress file exists and is empty, an error has occurred, then kill model run
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       cerr << "..progress XML file exists, but is empty => problem with model, quitting run" << '\n';
       return 1;
    } else if ( file_exists(progress_file) && !file_exists(rcf_file) ) {
       // Read contents of progress file
       read_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);
       // If last_iter less than the restart interval, then model is at beginning and rcf has yet to be produced then continue
       if (std::stoi(last_iter) >= restart_interval) {
          // Otherwise if progress file exists and rcf file does not exist, an error has occurred, then kill model run
          print_last_lines("NODE.001_01", 70);
          print_last_lines("ifs.stat",8);
          cerr << "..progress XML file exists, but rcf file does not exist => problem with model, quitting run" << '\n';
          return 1;
       } else {
          // Else model restarts from the beginning
          last_cpu_time = 0;
          upload_file_number = 0;
          last_iter = "0";
          last_upload = 0;
          model_completed = 0;
       }
    } else if ( !file_exists(progress_file) && file_exists(rcf_file) ) {
       // If rcf file exists and progress file does not exist, an error has occurred, then kill model run
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       cerr << "..rcf file exists, but progress XML file does not exist => problem with model, quitting run" << '\n';
       return 1;
    } else if ( (file_exists(progress_file) && !file_is_empty(progress_file)) && file_exists(rcf_file) ) {
       // If progress file exists and is not empty and rcf file exists, then read rcf file and progress file
       std::ifstream rcf_file_stream;
       std::string ctime_value = "", cstep_value = "";

       // Read the rcf file
       if( file_exists( rcf_file ) ) {
         if( !(rcf_file_stream.is_open()) ) {
            rcf_file_stream.open( rcf_file );
         }
         if( rcf_file_stream.is_open() ) {
            if (read_rcf_file(rcf_file_stream, ctime_value, cstep_value)) {
               cerr << "Read the rcf file" << '\n';
               //cerr << "rcf file CSTEP: " << cstep_value << '\n';
               //cerr << "rcf file CTIME: " << ctime_value << '\n';
            } else {
               // Reading the rcf file failed, then kill model run
               print_last_lines("NODE.001_01", 70);
               print_last_lines("ifs.stat",8);
               cerr << "..Reading the rcf file failed" << '\n';
	       return 1;
            }
         }
       }
       rcf_file_stream.close();

       // Read the progress file
       read_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

       // Check if the CSTEP variable from rcf is greater than the last_iter, if so then quit model run
       if ( stoi(cstep_value) > stoi(last_iter) ) {
          cerr << "..CSTEP variable from rcf is greater than last_iter from progress file, error has occurred, quitting model run" << '\n';
          return 1;
       }

       // Adjust last_iter to the step of the previous model restart dump step.
       // This is always a multiple of the restart frequency

       cerr << "-- Model is restarting --\n";
       cerr << "Adjusting last_iter, " << last_iter << ", to previous model restart step.\n";
       restart_iter = stoi(last_iter);
       restart_iter = restart_iter - ((restart_iter % restart_interval) - 1);   // -1 because the model will continue from restart_iter.
       last_iter = to_string(restart_iter); 
    }

    // Update progress file with current values
    update_progress_file(progress_file, current_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

    fraction_done = 0;
    trickle_upload_count = 0;

    // seconds between upload files: upload_interval
    // seconds between ICM files: ICM_file_interval * timestep_interval
    // upload interval in steps = upload_interval / timestep_interval
    //cerr "upload_interval: "<< upload_interval << ", timestep_interval: " << timestep_interval << '\n';

    // Check if upload_interval x timestep_interval equal to zero
    if (upload_interval * timestep_interval == 0) {
       cerr << "..upload_interval x timestep_interval equals zero" << std::endl;
       return 1;
    }

    int total_length_of_simulation = (int) (num_days * 86400);
    cerr << "total_length_of_simulation: " << total_length_of_simulation << '\n';

    // Get result_base_name to construct upload file names using 
    // the first upload as an example and then stripping off '_0.zip'

    if (!standalone) {
       retval = call_boinc_resolve_filename_s("upload_file_0.zip", resolved_name);
       if (retval) {
          cerr << "..boinc_resolve_filename failed" << std::endl;
	  return 1;
       }

       result_base_name = std::filesystem::path(resolved_name).stem(); // returns filename without path nor '.zip'
       if ( result_base_name.length() > 2 ){
          result_base_name.erase( result_base_name.length() - 2 );     // remove the '_0'
       }

       cerr << "result_base_name: " << result_base_name << '\n';
       if (result_base_name.compare("upload_file") == 0) {
          cerr << "..Failed to get result name" << std::endl;
          return 1;
       }
    }

    // Check for the existence of a Unix script file to override the environment variables
    // Script file should be in projects folder
    std::string override_env_vars = project_path + std::string("override_env_variables");
    if(file_exists(override_env_vars)) {
       // If exists then run file
       FILE* pipe = popen(override_env_vars.c_str(), "r");
       if (!pipe) {
          cerr << "..Failed to open environment variables override file" << std::endl;
          return 1;
       }
       pclose(pipe);
    }


    // Set the strCmd parameter
    if( file_exists( slot_path + std::string("/oifs_43r3_model.exe") ) ) {
       // Launch single process executable
       strCmd = slot_path + std::string("/oifs_43r3_model.exe");
    } else if( file_exists( slot_path + std::string("/oifs_43r3_omp_model.exe") ) ) {
       // Launch multi process executable
       strCmd = slot_path + std::string("/oifs_43r3_omp_model.exe");
    } else {
       // If no executable, then throw an error
       cerr << "..No executable present, ending model run" << std::endl;
       return 1;
    }

    // Start the OpenIFS job
    handleProcess = launch_process_oifs(slot_path, strCmd.c_str(), exptid.c_str(), app_name);
    if (handleProcess > 0) process_status = 0;

    call_boinc_end_critical_section();


    // process_status = 0 running
    // process_status = 1 stopped normally
    // process_status = 2 stopped with quit request from BOINC
    // process_status = 3 stopped with child process being killed
    // process_status = 4 stopped with child process being stopped
    // process_status = 5 child process not found by waitpid()


    //----------------------------------------Main loop------------------------------------------------------

    // Periodically check the process status and the BOINC client status
    std::string stat_lastline = "";

    while ((process_status == 0 && model_completed == 0) && skip_main_loop == 0) {
       sleep_until(system_clock::now() + seconds(1)); // Time gap of 1 second to reduce overhead of control code

       count++;

       // Check every 10 seconds whether an upload point has been reached
       if(count==10) {

          iter = last_iter;
          if( file_exists(slot_path + std::string("/ifs.stat")) ) {

             //  To reduce I/O, open file once only and use oifs_parse_ifsstat() to parse the step count
             if( !(ifs_stat_file.is_open()) ) {
                ifs_stat_file.open(slot_path + std::string("/ifs.stat"));
             } 
             if( ifs_stat_file.is_open() ) {

                // Read completed step from last line of ifs.stat file.
                // Note the first line from the model has a step count of '....  CNT3      -999 ....'
                // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
                // to the output files for that iteration, those files can now be moved and uploaded.

                oifs_get_stat(ifs_stat_file, stat_lastline);
                if ( oifs_parse_stat(stat_lastline, iter, 4) ) {     // iter updates
                   if ( !oifs_valid_step(iter,total_nsteps) ) {
                     iter = last_iter;
                   }
                }
             }
          } 

          if (std::stoi(iter) != std::stoi(last_iter)) {
             // Construct file name of the ICM result file
             second_part = get_second_part(last_iter, exptid);

             // Move the ICMGG result file to the temporary folder in the project directory
             first_part = "ICMGG";
             retval = move_result_file(slot_path, temp_path, first_part, second_part);
             if (retval) {
                cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
                return retval;
             }

             // Move the ICMSH result file to the temporary folder in the project directory
             first_part = "ICMSH";
             retval = move_result_file(slot_path, temp_path, first_part, second_part);
             if (retval) {
                cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
                return retval;
             }

             // Move the ICMUA result file to the temporary folder in the project directory
             first_part = "ICMUA";
             retval = move_result_file(slot_path, temp_path, first_part, second_part);
             if (retval) {
                cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
                return retval;
             }

             // Convert iteration number to seconds
             current_iter = (std::stoi(last_iter)) * timestep_interval;

             //cerr << "Current iteration of model: " << last_iter << '\n';
             //cerr << "timestep_interval: " << timestep_interval << '\n';
             //cerr << "current_iter: " << current_iter << '\n';
             //cerr << "last_upload: " << last_upload << '\n';

             // Upload a new upload file if the end of an upload_interval has been reached
             if((( current_iter - last_upload ) >= (upload_interval * timestep_interval)) && (current_iter < total_length_of_simulation)) {
                // Create an intermediate results zip file using BOINC zip
                zfl.clear();

                call_boinc_begin_critical_section();

                // Cycle through all the steps from the last upload to the current upload
                for (i = (last_upload / timestep_interval); i < (current_iter / timestep_interval); i++) {
                   //cerr << "last_upload/timestep_interval: " << (last_upload/timestep_interval) << '\n';
                   //cerr << "current_iter/timestep_interval: " << (current_iter/timestep_interval) << '\n';
                   //cerr << "i: " << (std::to_string(i)) << '\n';

                   // Construct file name of the ICM result file
                   second_part = get_second_part(std::to_string(i), exptid);

                   // Add ICMGG result files to zip to be uploaded
                   if(file_exists(temp_path + std::string("/ICMGG") + second_part)) {
                      cerr << "Adding to the zip: " << (temp_path + std::string("/ICMGG")+second_part) << '\n';
                      zfl.push_back(temp_path + std::string("/ICMGG") + second_part);
                      // Delete the file that has been added to the zip
                      // std::remove((temp_path+std::string("/ICMGG")+second_part).c_str());
                   }

                   // Add ICMSH result files to zip to be uploaded
                   if(file_exists(temp_path + std::string("/ICMSH") + second_part)) {
                      cerr << "Adding to the zip: " << (temp_path + std::string("/ICMSH")+second_part) << '\n';
                      zfl.push_back(temp_path + std::string("/ICMSH") + second_part);
                      // Delete the file that has been added to the zip
                      // std::remove((temp_path+std::string("/ICMSH")+second_part).c_str());
                   }

                   // Add ICMUA result files to zip to be uploaded
                   if(file_exists(temp_path + std::string("/ICMUA") + second_part)) {
                      cerr << "Adding to the zip: " << (temp_path + std::string("/ICMUA")+second_part) << '\n';
                      zfl.push_back(temp_path + std::string("/ICMUA") + second_part);
                      // Delete the file that has been added to the zip
                      // std::remove((temp_path+std::string("/ICMUA")+second_part).c_str());
                   }
                }

                // If running under a BOINC client
                if (!standalone) {

                   if (zfl.size() > 0){

                      // Create the zipped upload file from the list of files added to zfl
                      upload_file = project_path + result_base_name + "_" + std::to_string(upload_file_number) + ".zip";

                      cerr << "Zipping up the intermediate file: " << upload_file << '\n';
                      retval = call_boinc_zip(upload_file, &zfl);  // n.b. pass std::string to avoid copy-on-call

                      if (retval) {
                         cerr << "..Zipping up the intermediate file failed" << std::endl;
                         call_boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            std::remove(zfl[j].c_str());
                         }
                      }

                      // Upload the file. In BOINC the upload file is the logical name, not the physical name
                      upload_file_name = std::string("upload_file_") + std::to_string(upload_file_number) + std::string(".zip");
                      cerr << "Uploading the intermediate file: " << upload_file_name << '\n';
                      sleep_until(system_clock::now() + seconds(20));
                      call_boinc_upload_file(upload_file_name);
                      retval = call_boinc_upload_status(upload_file_name);
                      if (!retval) {
                         cerr << "Finished the upload of the intermediate file: " << upload_file_name << '\n';
                      }

                      trickle_upload_count++;
                      if (trickle_upload_count == 10) {
                        // Produce trickle
                        process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
                        trickle_upload_count = 0;
                      }
                   }
                   last_upload = current_iter; 
                }

                // Else running in standalone
                else {
                   upload_file_name = app_name + std::string("_") + unique_member_id + std::string("_") + start_date + std::string("_") + \
                               std::to_string(num_days_trunc) + std::string("_") + batchid + std::string("_") + wuid + std::string("_") + \
                               std::to_string(upload_file_number) + std::string(".zip");
                   cerr << "The current upload_file_name is: " << upload_file_name << '\n';

                   // Create the zipped upload file from the list of files added to zfl
                   upload_file = project_path + upload_file_name;

                   if (zfl.size() > 0){
                      retval = call_boinc_zip(upload_file, &zfl);

                      if (retval) {
                         cerr << "..Creating the zipped upload file failed" << std::endl;
                         call_boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            std::remove(zfl[j].c_str());
                         }
                      }
                   }
                   last_upload = current_iter;

                   trickle_upload_count++;
                   if (trickle_upload_count == trickle_upload_frequency) {
                      // Produce trickle
                      process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
                      trickle_upload_count = 0;
                   }

                }
                call_boinc_end_critical_section();
                upload_file_number++;
             }
          }
          last_iter = iter;
          count = 0;

          // Update progress file with current values
          update_progress_file(progress_file, current_cpu_time, upload_file_number, last_iter, last_upload, model_completed);
       }

       // Calculate current_cpu_time, only update if cpu_time returns a value
       if (cpu_time(handleProcess)) {
          current_cpu_time = last_cpu_time + cpu_time(handleProcess);
          //fprintf(stderr,"current_cpu_time: %1.5f\n",current_cpu_time);
       }


      // Calculate the fraction done
      fraction_done = model_frac_done( atof(iter.c_str()), total_nsteps, atoi(nthreads.c_str()) );
      //fprintf(stderr,"fraction done: %.6f\n", fraction_done);


      if (!standalone) {
         // If the current iteration is at a restart iteration     
         if (!(std::stoi(iter)%restart_interval)) restart_cpu_time = current_cpu_time;

         // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
         call_boinc_report_app_status(current_cpu_time, restart_cpu_time, fraction_done);

         // Provide the fraction done to the BOINC client, 
         // this is necessary for the percentage bar on the client
         call_boinc_fraction_done(fraction_done);

         // Check the status of the client if not in standalone mode     
         process_status = check_boinc_status(handleProcess,process_status);
      }

      // Check the status of the child process    
      process_status = check_child_status(handleProcess,process_status);
    }

    //-------------------------------------------------------------------------------------------------------	


    // Time delay to ensure model files are all flushed to disk
    sleep_until(system_clock::now() + seconds(60));

    // Print content of key model files to help with diagnosing problems
    print_last_lines("NODE.001_01", 70);    //  main model output log	

    // To check whether model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.
    if(file_exists(slot_path + std::string("/ifs.stat"))) {
       ifs_word="";
       oifs_get_stat(ifs_stat_file, last_line);
       oifs_parse_stat(last_line, ifs_word, 3);
       if (ifs_word!="CNT0") {
         cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
         // print extra files to help diagnose fail
         print_last_lines("ifs.stat",8);
         print_last_lines("rcf",11);              // openifs restart control
         print_last_lines("waminfo",17);          // wave model restart control
         print_last_lines(progress_file,8);
         cerr << "..Failed, model did not complete successfully" << std::endl;
         return 1;
       }
    }
    // ifs.stat has not been produced, then model did not start
    else {
       cerr << "..Failed, model did not start" << std::endl;
       return 1;	    
    }


    // Update model_completed
    model_completed = 1;

    // We need to handle the last ICM files
    // Construct final file name of the ICM result file
    second_part = get_second_part(last_iter, exptid);

    // Move the ICMGG result file to the temporary folder in the project directory
    first_part = "ICMGG";
    retval = move_result_file(slot_path, temp_path, first_part, second_part);
    if (retval) {
       cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
       return retval;
    }

    // Move the ICMSH result file to the temporary folder in the project directory
    first_part = "ICMSH";
    retval = move_result_file(slot_path, temp_path, first_part, second_part);
    if (retval) {
       cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
       return retval;
    }

    // Move the ICMUA result file to the temporary folder in the project directory
    first_part = "ICMUA";
    retval = move_result_file(slot_path, temp_path, first_part, second_part);
    if (retval) {
       cerr << "..Copying " << first_part << " result file to the temp folder in the projects directory failed" << "\n";
       return retval;
    }


    call_boinc_begin_critical_section();

    //-----------------------------Create the final results zip file-----------------------------------------

    zfl.clear();
    std::string node_file = slot_path + std::string("/NODE.001_01");
    zfl.push_back(node_file);
    std::string ifsstat_file = slot_path + std::string("/ifs.stat");
    zfl.push_back(ifsstat_file);
    cerr << "Adding to the zip: " << node_file << '\n';
    cerr << "Adding to the zip: " << ifsstat_file << '\n';

    // Read the remaining list of files from the slots directory and add the matching files to the list of files for the zip
    dirp = opendir(temp_path.c_str());
    if (dirp) {
        regcomp(&regex,"\\+",0);
        while ((dir = readdir(dirp)) != NULL) {
          //cerr << "In temp folder: "<< dir->d_name << '\n';

          if (!regexec(&regex,dir->d_name,(size_t) 0,NULL,0)) {
            zfl.push_back(temp_path + std::string("/") + dir->d_name);
            cerr << "Adding to the zip: " << (temp_path+std::string("/") + dir->d_name) << '\n';
          }
        }
        regfree(&regex);
        closedir(dirp);
    }

    // If running under a BOINC client
    if (!standalone) {
       if (zfl.size() > 0){

          // Create the zipped upload file from the list of files added to zfl
          upload_file = project_path + result_base_name + "_" + std::to_string(upload_file_number) + ".zip";

          cerr << "Zipping up the final file: " << upload_file << '\n';
          retval = call_boinc_zip(upload_file, &zfl);

          if (retval) {
             cerr << "..Zipping up the final file failed" << std::endl;
             call_boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                std::remove(zfl[j].c_str());
             }
          }

          // Upload the file. In BOINC the upload file is the logical name, not the physical name
          upload_file_name = std::string("upload_file_") + std::to_string(upload_file_number) + std::string(".zip");
          cerr << "Uploading the final file: " << upload_file_name << '\n';
          sleep_until(system_clock::now() + seconds(20));
          call_boinc_upload_file(upload_file_name);
          retval = call_boinc_upload_status(upload_file_name);
          if (!retval) {
             cerr << "Finished the upload of the final file" << '\n';
          }

	  // Produce trickle
          process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
       }
       call_boinc_end_critical_section();
    }
    // Else running in standalone
    else {
       upload_file_name = app_name + std::string("_") + unique_member_id + std::string("_") + start_date + std::string("_") + \
                   std::to_string(num_days_trunc) + std::string("_") + batchid + std::string("_") + wuid + std::string("_") + \
                   std::to_string(upload_file_number) + std::string(".zip");
       cerr << "The final upload_file_name is: " << upload_file_name << '\n';

       // Create the zipped upload file from the list of files added to zfl
       upload_file = project_path + upload_file_name;

       if (zfl.size() > 0){
          retval = call_boinc_zip(upload_file, &zfl);
          if (retval) {
             cerr << "..Creating the zipped upload file failed" << std::endl;
             call_boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                std::remove(zfl[j].c_str());
             }
          }
        }
	// Produce trickle
        process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);     
    }

    //-------------------------------------------------------------------------------------------------------


    // Now that the task has finished, remove the temp folder
    std::remove(temp_path.c_str());

    sleep_until(system_clock::now() + seconds(120));

    // if finished normally
    if (process_status == 1){
      call_boinc_end_critical_section();
      call_boinc_finish(0);
      cerr << "Task finished" << std::endl;
      return 0;
    }
    else if (process_status == 2){
      call_boinc_end_critical_section();
      call_boinc_finish(0);
      cerr << "Task finished" << std::endl;
      return 0;
    }
    else {
      call_boinc_end_critical_section();
      call_boinc_finish(1);
      cerr << "Task finished" << std::endl;
      return 1;
    }	
}
