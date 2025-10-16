//
// Control code for the OpenIFS application in the climateprediction.net project
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) December 2023
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include "CPDN_control_code.h"


// Set the required OpenIFS environment variables
bool oifs_setenvs(const std::string& slot_path, const std::string& nthreads) {

    // For memory safety, keep env strings static so they persist for the life of the program.

    // Set the OIFS_DUMMY_ACTION environmental variable, this controls what OpenIFS does if it goes into a dummy subroutine
    // Possible values are: 'quiet', 'verbose' or 'abort'
    if ( !set_env_var("OIFS_DUMMY_ACTION", "abort") ) {
      std::cerr << "..Setting the OIFS_DUMMY_ACTION environmental variable failed" << std::endl;
      return false;
    }

    // Set the OMP_NUM_THREADS environmental variable; nthreads must be a positive integer string
    if ( !set_env_var("OMP_NUM_THREADS", nthreads) ) {
      std::cerr << "..Setting the OMP_NUM_THREADS environmental variable failed" << std::endl;
      return false;
    }
    std::cerr << "Info: OMP_NUM_THREADS is set to: " << getenv("OMP_NUM_THREADS") << "\n";

    // Set the OMP_SCHEDULE environmental variable, this enforces static thread scheduling
    if ( !set_env_var("OMP_SCHEDULE", "STATIC") ) {
      std::cerr << "..Setting the OMP_SCHEDULE environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK environmental variable, this controls the tracing facility in OpenIFS, off=0 and on=1
    if ( !set_env_var("DR_HOOK", "1") ) {
      std::cerr << "..Setting the DR_HOOK environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK_HEAPCHECK environmental variable, this ensures the heap size statistics are reported
    if ( !set_env_var("DR_HOOK_HEAPCHECK", "no") ) {
      std::cerr << "..Setting the DR_HOOK_HEAPCHECK environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK_STACKCHECK environmental variable, this ensures the stack size statistics are reported
    if ( !set_env_var("DR_HOOK_STACKCHECK", "no") ) {
      std::cerr << "..Setting the DR_HOOK_STACKCHECK environmental variable failed" << std::endl;
      return false;
    }

    // Set the EC_MEMINFO environment variable, only applies to OpenIFS 43r3.
    // Disable EC_MEMINFO to remove the useless EC_MEMINFO messages to the stdout file to reduce filesize.
    if ( !set_env_var("EC_MEMINFO", "0") ) {
       std::cerr << "..Setting the EC_MEMINFO environment variable failed" << std::endl;
       return false;
    }

    // Disable Heap memory stats at end of run; does not work for CPDN version of OpenIFS
    if ( !set_env_var("EC_PROFILE_HEAP", "0") ) {
       std::cerr << "..Setting the EC_PROFILE_HEAP environment variable failed" << std::endl;
       return false;
    }

    // Disable all memory stats at end of run; does not work for CPDN version of OpenIFS
    if ( !set_env_var("EC_PROFILE_MEM", "0") ) {
       std::cerr << "..Setting the EC_PROFILE_MEM environment variable failed" << std::endl;
       return false;
    }

    // Set the OMP_STACKSIZE environmental variable, OpenIFS needs more stack memory per process
    if ( !set_env_var("OMP_STACKSIZE", "128M") ) {
      std::cerr << "..Setting the OMP_STACKSIZE environmental variable failed" << std::endl;
      return false;
    }

    // Set the GRIB_SAMPLES_PATH environmental variable
    std::string GRIB_SAMPLES_var = slot_path + "/eccodes/ifs_samples/grib1_mlgrib2";
    if ( !set_env_var("GRIB_SAMPLES_PATH", GRIB_SAMPLES_var) )  {
      std::cerr << "..Setting the GRIB_SAMPLES_PATH failed" << std::endl;
      return false;
    }
    std::cerr << "The GRIB_SAMPLES_PATH environmental variable is: " << getenv("GRIB_SAMPLES_PATH") << "\n";

    // Set the GRIB_DEFINITION_PATH environmental variable
    std::string GRIB_DEF_var = slot_path + "/eccodes/definitions";
    if ( !set_env_var("GRIB_DEFINITION_PATH", GRIB_DEF_var) )  {
      std::cerr << "..Setting the GRIB_DEFINITION_PATH failed" << std::endl;
      return false;
    }
    std::cerr << "The GRIB_DEFINITION_PATH environmental variable is: " << getenv("GRIB_DEFINITION_PATH") << "\n";

    return true;
}


int main(int argc, char** argv)
{
    int retval=0;

    // Argument processing; at least 9 args always.
    if (argc < 9) {
        std::cerr << "Control code error: Not enough command line arguments provided.\n"
                  << "Usage: " << argv[0] << " <start_date> <exptid> <unique_member_id> <batchid> <wuid> <fclen> <app_name> <nthreads> [app_version]\n";
        return 1;
    }
    std::cerr << "(argv0) " << argv[0] << '\n'
              << "(argv1) start_date: " << argv[1] << '\n'
              << "(argv2) exptid: " << argv[2] << '\n'
              << "(argv3) unique_member_id: " << argv[3] << '\n'
              << "(argv4) batchid: " << argv[4] << '\n'
              << "(argv5) wuid: " << argv[5] << '\n'
              << "(argv6) fclen: " << argv[6] << '\n'
              << "(argv7) app_name: " << argv[7] << '\n'
              << "(argv8) nthreads: " << argv[8] << std::endl;

    // Read the exptid, umid, batchid, wuid, fclen, app_name, number of threads from the command line
    // argv[9] if present, is assigned below
    std::string start_date = argv[1]; // simulation start date
    std::string exptid = argv[2];     // OpenIFS experiment id
    std::string unique_member_id = argv[3];  // umid
    std::string batchid = argv[4];    // batch id
    std::string wuid = argv[5];       // workunit id
    std::string fclen = argv[6];      // number of simulation days
    std::string app_name = argv[7];   // CPDN app name
    std::string nthreads = argv[8];   // number of OPENMP threads.
    std::string app_config_nthreads;  // blank initially.

    // Check for optional '--nthreads <value>' at end of arg list, optionally set by app_config.xml on user's machine.
    if ( std::string(argv[argc - 2]) == "--nthreads" ) {
      app_config_nthreads = argv[argc-1];

      if ( app_config_nthreads.empty() ) {
         std::cerr << "Warning. --nthreads argument present but has no value! Ignoring.\n";
      }
      else {
         try {
            int max_nthreads = 8;      // GC. This is the best maximum as T319 parallel efficiency markedly drops after this many threads.
            int min_nthreads = 2;
            int i_nthreads = -1;

            i_nthreads = std::stoi(app_config_nthreads);
            if ( i_nthreads > max_nthreads ) {
               std::cerr << "Warning. --nthreads value is too high. Setting to max number of threads : " << max_nthreads << '\n';
               i_nthreads = max_nthreads;
            }
            else if ( i_nthreads < min_nthreads ) {
               std::cerr << "Warning. --nthreads is too low for this configuration. Minimum #threads is 2. Resetting.\n";
               i_nthreads = min_nthreads;
            }
            else {
               //*****************************************************
               // Uncomment this line to enable the --nthreads argument
               //nthreads = i_nthreads
            }
         }
         catch (...) {
            std::cerr << "Warning. --nthreads argument must be a valid integer! Ignoring.\n";
         }
      }
    }

    // Initialise BOINC to get the project directory, workunit name and app version
    std::string project_path;
    std::string wu_name;
    std::string project_dir;
    std::string version;
    int standalone=0;

    retval = initialise_boinc(wu_name, project_dir, version, standalone);
    if (retval) {
       std::cerr << "..BOINC initialisation failed" << "\n";
       return retval;
    }

    std::cerr << "Control Code version: " << CODE_VERSION << '\n' // CODE_VERSION is a macro set at compile time
              << "wu_name: " << wu_name << '\n'
              << "project_dir: " << project_dir << '\n'
              << "version: " << version << '\n';

    const std::string namelist="fort.4";    // namelist file

    double num_days = atof(fclen.c_str()); // number of simulation days
    int num_days_trunc = (int) num_days; // number of simulation days truncated to an integer

    // Get the slots path (the current working path)
    std::string slot_path = fs::current_path();
    if (slot_path.empty()) {
      std::cerr << "..current_path() returned empty" << std::endl;
    }
    else {
      std::cerr << "Working directory is: "<< slot_path << '\n';      
    }

    if (!standalone) {

      // Get the project path
      project_path = project_dir + std::string("/");
      std::cerr << "Project directory is: " << project_path << '\n';

      // Get the app version and re-parse to add a dot
      // GC. This assumes version is X.Y, X.YY or XX.YY format, will get it wrong if not.
      auto vlen = version.length();
      if ( vlen == 2 ) {
         version.insert(1, ".");
      }
      else if (vlen > 2 ) {
         version.insert(vlen-2, ".");
      }

      std::cerr << "app name: " << app_name << '\n'
                << "version: " << version << '\n';
    }
    // Running in standalone
    else {
      std::cerr << "Running in standalone mode" << '\n';
      // Set the project path
      project_path = slot_path + std::string("/../projects/");
      std::cerr << "Project directory is: " << project_path << '\n';

      // In standalone get the app version from the command line
      version = argv[9];
      std::cerr << "app name: " << app_name << '\n'
                << "(argv9) app_version: " << argv[9] << '\n'; 
    }

    boinc_begin_critical_section();

    // Create temporary folder for moving the results to and uploading the results from
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string temp_path = project_path + app_name + "_" + wuid;
    std::cerr << "Location of temp folder: " << temp_path << '\n';
    if ( !file_exists(temp_path) ) {
      if (mkdir(temp_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) std::cerr << "..mkdir for temp folder for results failed" << std::endl;
    }

    // Move and unzip app file
    retval = move_and_unzip_app_file(app_name, version, project_path, slot_path);
    if (retval) {
      std::cerr << "..move_and_unzip_app_file failed" << "\n";
      return retval;
    }


    //------------------------------------------Process the namelist-----------------------------------------
    // GC. Note, this is not the 'model fort.4' namelist file being referred to here. Needs renaming to avoid confusion.
    
   fs::path namelist_zip_path = slot_path;
   namelist_zip_path /= std::string(app_name) + "_" +
                       unique_member_id + "_" +
                       start_date + "_" +
                       std::to_string(num_days_trunc) + "_" +
                       batchid + "_" +
                       wuid + ".zip";
   std::string namelist_zip = namelist_zip_path.string();      // nb this is a const string.

	// Copy the namelist_zip to the slot directory and unzip
    if ( copy_and_unzip(namelist_zip, namelist_zip, slot_path, "namelist_zip") ) {
       std::cerr << "..Copying and unzipping the namelist_zip failed: " << namelist_zip << std::endl;
       return 1;        // should terminate, the model won't run.
	}


    // Parse the fort.4 namelist for the filenames and variables
    std::string ifsdata_file;
    std::string ic_ancil_file;
    std::string climate_data_file;
    std::string namelist_file = slot_path + "/" + namelist;
    std::string namelist_line;
    std::string horiz_resolution;
    std::string vert_resolution;
    std::string grid_type;
    std::string tmpstr;
    std::ifstream namelist_filestream;
    char equals = '=';

    int upload_interval = 0;
    int trickle_upload_frequency = 0;
    int timestep_interval = 0;
    int ICM_file_interval = 0;
    int restart_interval = 0;
    

    // Check for the existence of the namelist
    if( !file_exists(namelist_file) ) {
       std::cerr << "..The namelist file does not exist: " << namelist_file << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Open the namelist file
    if(!(namelist_filestream.is_open())) {
       namelist_filestream.open(namelist_file);
    }

    // Read the namelist file
    // GC. Recoded. Removed unneccesary use of istringstream, moved code to new function, and fixed incorrect length used in substr().
    //     Also fixed bug reading string '!NFRPOS', should have been 'NFRPOS', meaning it was never assigned correct value.
    while(std::getline(namelist_filestream, namelist_line))
    {
       tmpstr.clear();

       if ( extract_key_value( namelist_line,"IFSDATA_FILE", equals, ifsdata_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "IC_ANCIL_FILE", equals, ic_ancil_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "CLIMATE_DATA_FILE", equals, climate_data_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "HORIZ_RESOLUTION", equals, horiz_resolution ) ) {
       }
       else if ( extract_key_value( namelist_line, "VERT_RESOLUTION", equals, vert_resolution ) ) {
       }
       else if ( extract_key_value( namelist_line, "GRID_TYPE", equals, grid_type ) ) {
       }
       else if ( extract_key_value( namelist_line, "UPLOAD_INTERVAL", equals, tmpstr ) ) {
          try {
            upload_interval=std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse upload interval from namelist, setting to zero, got string: " << tmpstr << '\n';
            upload_interval = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "TRICKLE_UPLOAD_FREQUENCY", equals, tmpstr ) ) {
          try {
            trickle_upload_frequency=std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse trickle upload frequency from namelist, setting to zero, got string: " << tmpstr << '\n';
            trickle_upload_frequency = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "UTSTEP", equals, tmpstr) ) {
          try {
            timestep_interval = std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse timestep interval from namelist, setting to zero, got string: " << tmpstr << '\n';
            timestep_interval = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "NFRPOS", equals, tmpstr) ) {   // frequency of model OUTPUT file creation (for upload); +ve model steps, -ve hours.
          try {
            ICM_file_interval = std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse ICM model output interval from namelist, setting to zero, got string: " << tmpstr << std::endl;
            ICM_file_interval = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "NFRRES", equals, tmpstr) ) {     // frequency of model RESTART file creation: +ve model steps, -ve hours.
          try {
            restart_interval = stoi(tmpstr);
          }
          catch (...) {
            std::cerr << "..Warning, unable to parse restart interval from namelist, setting to zero, got string: " << tmpstr << std::endl;
            restart_interval = 0;
          }
       }
    }
    namelist_filestream.close();

    // Check for any empty variables in case parsing failed.
    // These might case the task to fail. Integer values checked above.
    if ( ifsdata_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse ifs_data_file from namelist." << '\n';
    }
    if ( ic_ancil_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse ic_ancil_file from namelist." << '\n';
    }
    if ( climate_data_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse climate_data_file from namelist." << '\n';
    }
    if ( horiz_resolution.empty() ) {
      std::cerr << ".. Warning. Unable to parse horiz_resolution from namelist." << '\n';
    }
    if ( vert_resolution.empty() ) {
      std::cerr << ".. Warning. Unable to parse vert_resolution from namelist." << '\n';
    }
    if ( grid_type.empty() ) {
      std::cerr << ".. Warning. Unable to parse grid_type from namelist." << '\n';
    }

    std::cerr << "Values read from model namelist are: \n"
               << " ifsdata_file: " << ifsdata_file << '\n'
               << " ic_ancil_file: " << ic_ancil_file << '\n'
               << " climate_data_file: " << climate_data_file << '\n'
               << " horiz_resolution: " << horiz_resolution << '\n'
               << " vert_resolution: " << vert_resolution << '\n'
               << " grid_type: " << grid_type << '\n'
               << " Upload_interval: " << upload_interval << '\n'
               << " Trickle_upload_frequency: " << trickle_upload_frequency << '\n'
               << " UTSTEP: " << timestep_interval << '\n'
               << " NFRPOS: " << ICM_file_interval << '\n'
               << " NFFRES: " << restart_interval << std::endl;

    //-------------------------------------------------------------------------------------------------------

    // restart frequency might be in units of hrs, convert to model steps
    if ( restart_interval < 0 ) {
       restart_interval = abs(restart_interval)*3600 / timestep_interval;
    }
    std::cerr << "nfrres: restart dump frequency (steps) " << restart_interval << '\n';

    // this should match CUSTEP in fort.4. If it doesn't we have a problem
    double total_nsteps = (num_days * 86400.0) / (double) timestep_interval;

    // Process the ic_ancil_file:
    std::string ic_ancil_zip = slot_path + "/" + ic_ancil_file + ".zip";

	// Copy the ic_ancil_zip to the slot directory and unzip
    if ( copy_and_unzip(ic_ancil_zip, ic_ancil_zip, slot_path, "ic_ancil_zip") ) {
       std::cerr << "..Copying and unzipping the ic_ancil_zip failed: " << ic_ancil_zip << std::endl;
       return 1;        // should terminate, the model won't run.
	}

    // Process the ifsdata_file:
    // Make the ifsdata directory and set the required paths
    std::string ifsdata_folder = slot_path + "/ifsdata";
    std::string ifsdata_zip    = slot_path + "/" + ifsdata_file + ".zip";
    std::string ifsdata_destination = ifsdata_folder + "/" + ifsdata_file + ".zip";
    
    // Check if ifsdata folder does not already exists or is empty
    if ( !file_exists(ifsdata_folder) ) {
       if (mkdir(ifsdata_folder.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) {
          std::cerr << "..mkdir for ifsdata folder failed" << std::endl;
          return 1;        // should terminate, the model won't run.
       }
    }
    
    // Copy the ifsdata_zip to the slot directory and unzip
    // GC TODO. convert to fs::path and get rid of handling '/'
    std::string ifsdata_check = ifsdata_folder + "/";
    if ( copy_and_unzip(ifsdata_zip, ifsdata_destination, ifsdata_check, "ifsdata_zip") ) {
       std::cerr << "..Copying and unzipping the ifsdata_zip failed: " << ifsdata_zip << std::endl;
       return 1;        // should terminate, the model won't run.
    }


    // Process the climate_data_file:
    // Make the climate data directory and set the required paths
    std::string climate_data_path = slot_path + "/" + horiz_resolution + grid_type;
    std::string climate_data_zip = slot_path + "/" + climate_data_file + ".zip";
    std::string climate_data_destination = climate_data_path + "/" + climate_data_file + ".zip";
    
    // Check if climate_data folder does not already exists or is empty
    if ( !file_exists(climate_data_path) ) {
       if (mkdir(climate_data_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) {
          std::cerr << "..mkdir for the climate data folder failed" << std::endl;
          return 1;
       }
    }               
       
    // Copy the climate_data_zip to the slot directory and unzip
    if ( copy_and_unzip(climate_data_zip, climate_data_destination, climate_data_path, "climate_data_zip") ) {
       std::cerr << "..Copying and unzipping the climate_data_zip failed: " << climate_data_zip << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    //-------------------------------------------------------------------------------------------------------

    // Set the core dump size to 0
    struct rlimit core_limits;
    core_limits.rlim_cur = core_limits.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &core_limits) != 0) {
       std::cerr << "..Setting the core dump size to 0 failed" << std::endl;
       return 1;
    }

    // Set the stack limit to be unlimited
    struct rlimit stack_limits;
    // In macOS we cannot set the stack size limit to infinity
    #ifndef __APPLE__ // Linux
       stack_limits.rlim_cur = stack_limits.rlim_max = RLIM_INFINITY;
       if (setrlimit(RLIMIT_STACK, &stack_limits) != 0) {
          std::cerr << "..Setting the stack limit to unlimited failed" << std::endl;
          return 1;
       }
    #endif


    // Define the name and location of the progress file and the rcf file
    std::string progress_file = slot_path + "/progress_file_" + wuid;
    std::string rcf_file = slot_path + "/rcf";

    std::string last_iter = "0";

    int model_completed = 0;  // Indicates model state; 0=not completed, 1=completed
    int last_upload;          // The time of the last upload file (in seconds)
    int upload_file_number = 0;
    int last_cpu_time = 0;

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    std::cerr << "Checking for rcf file and progress file: " << progress_file << '\n';

    // Handle the cases of the various states of the rcf file and progress file
    if ( !file_exists(progress_file) && !file_exists(rcf_file) ) {
       // If both progress file and rcf file do not exist, then model has not run, then set the initial values of run
       last_cpu_time = 0;
       upload_file_number = 0;
       last_iter = "0";
       last_upload = 0;
       model_completed = 0;
    }
    else if ( file_exists(progress_file) && file_is_empty(progress_file) ) {
       // If progress file exists and is empty, an error has occurred, then kill model run
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       std::cerr << "..progress file exists, but is empty => problem with model, quitting run" << '\n';
       return 1;
    }
    else if ( file_exists(progress_file) && !file_exists(rcf_file) ) {
       // Read contents of progress file
       read_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);
       // If last_iter less than the restart interval, then model is at beginning and rcf has yet to be produced then continue
       if (std::stoi(last_iter) >= restart_interval) {
          // Otherwise if progress file exists and rcf file does not exist, an error has occurred, then kill model run
          print_last_lines("NODE.001_01", 70);
          print_last_lines("ifs.stat",8);
          std::cerr << "..progress file exists, but rcf file does not exist => problem with model, quitting run" << '\n';
          return 1;
       }
       else {
          // Else model restarts from the beginning
          last_cpu_time = 0;
          upload_file_number = 0;
          last_iter = "0";
          last_upload = 0;
          model_completed = 0;
       }
    }
    else if ( !file_exists(progress_file) && file_exists(rcf_file) ) {
       // If rcf file exists and progress file does not exist, an error has occurred, then kill model run
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       std::cerr << "..rcf file exists, but progress file does not exist => problem with model, quitting run" << '\n';
       return 1;
    }
    else if ( (file_exists(progress_file) && !file_is_empty(progress_file)) && file_exists(rcf_file) ) {
       // If progress file exists and is not empty and rcf file exists, then read rcf file and progress file
       std::ifstream rcf_file_stream;
       std::string ctime_value;
       std::string cstep_value;

       // Read the rcf file
       if( file_exists( rcf_file ) ) {
         if( !(rcf_file_stream.is_open()) ) {
            rcf_file_stream.open( rcf_file );
         }
         if( rcf_file_stream.is_open() ) {
            if (read_rcf_file(rcf_file_stream, ctime_value, cstep_value)) {
               std::cerr << "Read the rcf file" << '\n';
               //std::cerr << "rcf file CSTEP: " << cstep_value << '\n';
               //std::cerr << "rcf file CTIME: " << ctime_value << '\n';
            }
            else {
               // Reading the rcf file failed, then kill model run
               print_last_lines("NODE.001_01", 70);
               print_last_lines("ifs.stat",8);
               std::cerr << "..Reading the rcf file failed" << '\n';
	            return 1;
            }
         }
       }
       rcf_file_stream.close();

       // Read the progress file
       read_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

       // Check if the CSTEP variable from rcf is greater than the last_iter, if so then quit model run
       if ( stoi(cstep_value) > stoi(last_iter) ) {
          std::cerr << "..CSTEP variable from rcf is greater than last_iter from progress file, error has occurred, quitting model run" << '\n';
          return 1;
       }

       // Adjust last_iter to the step of the previous model restart dump step.
       // This is always a multiple of the restart frequency

       std::cerr << "-- Model is restarting --\n";
       std::cerr << "Adjusting last_iter, " << last_iter << ", to previous model restart step.\n";
       int restart_iter = stoi(last_iter);
       restart_iter = restart_iter - ((restart_iter % restart_interval) - 1);   // -1 because the model will continue from restart_iter.
       last_iter = std::to_string(restart_iter); 
    }

    // Update progress file with current values
    double current_cpu_time = 0;
    double fraction_done = 0;

    int trickle_upload_count = 0;

    update_progress_file(progress_file, current_cpu_time, upload_file_number, last_iter, last_upload, model_completed);


    // seconds between upload files: upload_interval
    // seconds between ICM files: ICM_file_interval * timestep_interval
    // upload interval in steps = upload_interval / timestep_interval
    //cerr "upload_interval: "<< upload_interval << ", timestep_interval: " << timestep_interval << '\n';

    // Check if upload_interval x timestep_interval equal to zero
    if (upload_interval * timestep_interval == 0) {
       std::cerr << "..upload_interval x timestep_interval equals zero" << std::endl;
       return 1;
    }

    int total_length_of_simulation = (int) (num_days * 86400);
    std::cerr << "total_length_of_simulation: " << total_length_of_simulation << '\n';

    // Get result_base_name to construct upload file names using 
    // the first upload as an example and then stripping off '_0.zip'

    std::string result_base_name;

    if (!standalone) {
       std::string resolved_name;
       retval = boinc_resolve_filename_s("upload_file_0.zip", resolved_name);
       if (retval) {
          std::cerr << "..boinc_resolve_filename failed" << std::endl;
          return 1;
       }

       result_base_name = fs::path(resolved_name).stem(); // returns filename without path nor '.zip'
       if ( result_base_name.length() > 2 ){
          result_base_name.erase( result_base_name.length() - 2 );     // remove the '_0'
       }

       std::cerr << "result_base_name: " << result_base_name << '\n';
       if (result_base_name.compare("upload_file") == 0) {
          std::cerr << "..Failed to get result name" << std::endl;
          return 1;
       }
    }

    // Determine which OpenIFS executable to run.
    // GC. This should be an input parameter on the command line.

    fs::path single_proc_exe = slot_path;
             single_proc_exe /= "oifs_43r3_model.exe";
    fs::path multi_proc_exe  = slot_path;
             multi_proc_exe /= "oifs_43r3_omp_model.exe";
    fs::path test_proc_exe   = slot_path;
             test_proc_exe /= "oifs_43r3_test.exe";
    std::string exe_cmd{};

    if ( file_exists(single_proc_exe.string()) ) {
       exe_cmd = single_proc_exe.string();
    }
    else if ( file_exists(multi_proc_exe.string()) ) {
       exe_cmd = multi_proc_exe.string();
    }
    else if ( file_exists(test_proc_exe.string()) ) {
       exe_cmd = test_proc_exe.string();
    }
    if (exe_cmd.empty()) {
       std::cerr << "..No OpenIFS executable found, ending task." << std::endl;
       return 1;
    }

    // Bug workaround. The current cpdn_unzip function does not preserve executable permissions on Linux.
    // Manually set the permissions on the OpenIFS executable before running.

    if ( !set_exec_perms(exe_cmd) ) {
       std::cerr << "..Cannot start model. Setting execute permission for OpenIFS executable failed: " << exe_cmd << std::endl;
       return 1;
    }

    // Start the OpenIFS job
    int process_status=1;

    std::cerr << "Launching OpenIFS executable: " << exe_cmd << std::endl;
    long handleProcess = launch_process_oifs(project_path, slot_path, exe_cmd, nthreads, exptid, app_name);
    if (handleProcess > 0) process_status = 0;     //GC TODO. Need to handle when handleProcess =-1, i.e. launch failed (see code in launch_process_oifs)

    boinc_end_critical_section();


    // process_status = 0 running
    // process_status = 1 stopped normally
    // process_status = 2 stopped with quit request from BOINC
    // process_status = 3 stopped with child process being killed
    // process_status = 4 stopped with child process being stopped
    // process_status = 5 child process not found by waitpid()


    //----------------------------------------Main loop------------------------------------------------------

    // Periodically check the process status and the BOINC client status
    std::string stat_lastline;
    std::string second_part;
    std::string ifs_stat = slot_path + "/ifs.stat";     // GC. TODO: should be std::filesystem path.

    std::vector<fs::path> zfl;

    int count = 0;
    int current_iter = 0;

    while (process_status == 0 && model_completed == 0)
    {
       std::string iter = "0";

       std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(1)); // Time gap of 1 second to reduce overhead of control code

       count++;

       // Check whether an upload point has been reached
       // GC. 09/25. reduced to 7 secs as testing shows 10secs can miss a timestep.
       // Going too low can cause the %age done on boincmgr to flip backwards.
       if(count==7) {

          iter = last_iter;
          if ( file_exists(ifs_stat) ) {

             // Read completed step from last line of ifs.stat file.
             // Note the first line from the model has a step count of '....  CNT3      -999 ....'
             // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
             // to the output files for that iteration, those files can now be moved and uploaded.
             //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

             if ( fread_last_line(ifs_stat, stat_lastline) ) {       // only returns true if lastline has changed
                 if ( oifs_parse_stat(stat_lastline, iter, 4) ) {    // iter updates
                    if ( !oifs_valid_step(iter,total_nsteps) ) {
                       iter = last_iter;                             // revert to last valid step
                    }
                 }
             }
          }

          //std::cerr << "Checking whether a new set of ICM files have been generated: iter, last_iter = " << iter << ", " << last_iter << std::endl;

          if (std::stoi(iter) != std::stoi(last_iter)) {
             // Construct file name of the ICM result file
             second_part = get_second_part(last_iter, exptid);

             // Move the ICMGG, ICMSH & ICMUA result files to the task folder in the project directory
             std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
             for (const auto& part : icm) {
                  retval = move_result_file(slot_path, temp_path, part, second_part);
                  if (retval) {
                     std::cerr << "..Copying " << part << " result file to the temp folder in the projects directory failed" << "\n";
                     return retval;
                  }
             }

             // Convert iteration number to seconds
             current_iter = (std::stoi(last_iter)) * timestep_interval;

             //std::cerr << "Current iteration of model: " << last_iter << '\n';
             //std::cerr << "timestep_interval: " << timestep_interval << '\n';
             //std::cerr << "current_iter: " << current_iter << '\n';
             //std::cerr << "last_upload: " << last_upload << '\n';

             // Upload a new upload file if the end of an upload_interval has been reached
             if((( current_iter - last_upload ) >= (upload_interval * timestep_interval)) && (current_iter < total_length_of_simulation)) {
                // Create an intermediate results zip file
                zfl.clear();

                std::cerr << "End of upload interval reached, starting a new upload process" << std::endl;

                // *****  Critical section -- all returns must now call boinc_end_critical_section()  *****
                boinc_begin_critical_section();

                // Cycle through all the steps from the last upload to the current upload
                for (auto i = (last_upload / timestep_interval); i < (current_iter / timestep_interval); i++) {
                   //std::cerr << "last_upload/timestep_interval: " << (last_upload/timestep_interval) << '\n';
                   //std::cerr << "current_iter/timestep_interval: " << (current_iter/timestep_interval) << '\n';
                   //std::cerr << "i: " << (std::to_string(i)) << '\n';

                   // Construct file name of the ICM result file
                   second_part = get_second_part(std::to_string(i), exptid);

                   // Add ICM result files to zip to be uploaded
                   std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
                   for (const auto& part : icm) {
                      std::string fpath = temp_path + "/" + part + second_part;
                      if (file_exists(fpath)) {
                         std::cerr << "Adding to the zip: " << fpath << '\n';
                         zfl.push_back(fpath);
                      }
                   }
                }

                // If running under a BOINC client
                if (!standalone) {

                   if (zfl.size() > 0)
                   {
                      // Create the zipped upload file from the list of files added to zfl
                      std::string upload_file = project_path + result_base_name + "_" + std::to_string(upload_file_number) + ".zip";

                      std::cerr << "Compressing upload file: " << upload_file << '\n';

                      // Time the compression for diagnostics
                      auto start = chrono::high_resolution_clock::now();
                      auto outcome = cpdn_zip(upload_file, zfl);
                      auto stop = chrono::high_resolution_clock::now();
                      auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
                      std::cerr << "Time taken to compress upload file: " << duration.count() << " ms\n";

                      retval = outcome ? 0 : 1;

                      if (retval) {
                         std::cerr << ".. compressing upload file failed" << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (auto j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            try {
                                fs::remove(zfl[j]);
                            } catch (const fs::filesystem_error& e) {
                                std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                            }
                         }
                      }

                      // Upload the file. In BOINC the upload file is the logical name, not the physical name
                      std::string upload_file_name = "upload_file_" + std::to_string(upload_file_number) + ".zip";
                      std::cerr << "Uploading the intermediate file: " << upload_file_name << '\n';
                      std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(20));
                      retval = boinc_upload_file(upload_file_name);
                      if (retval) {
                         std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      retval = boinc_upload_status(upload_file_name);
                      if (!retval) {
                         std::cerr << "Finished the upload of the intermediate file: " << upload_file_name << '\n';
                      }

                      trickle_upload_count++;
                      if (trickle_upload_count == 10) {
                        // Produce trickle
                        std::cerr << "Producing trickle" << std::endl;
                        process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
                        trickle_upload_count = 0;
                      }
                   }
                   last_upload = current_iter; 
                }

                // Else running in standalone
                else {
                   std::string upload_file_name = app_name + "_" + unique_member_id + "_" + start_date + "_" + \
                                                  std::to_string(num_days_trunc) + "_" + batchid + "_" + wuid + "_" + \
                                                  std::to_string(upload_file_number) + ".zip";
                   std::cerr << "The current upload_file_name is: " << upload_file_name << '\n';

                   // Create the zipped upload file from the list of files added to zfl
                   std::string upload_file = project_path + upload_file_name;

                   if (zfl.size() > 0){
                      if (!cpdn_zip(upload_file, zfl)) {
                         retval = 1;
                      }

                      if (retval) {
                         std::cerr << "..Creating the zipped upload file failed" << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (auto j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            try {
                                fs::remove(zfl[j]);
                            } catch (const fs::filesystem_error& e) {
                                std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                            }
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

                // *****  Normal end of critical section  *****
                boinc_end_critical_section();
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
         double restart_cpu_time = 0;
         if ( !(std::stoi(iter)%restart_interval)) {
            restart_cpu_time = current_cpu_time;
         }

         // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
         boinc_report_app_status(current_cpu_time, restart_cpu_time, fraction_done);

         // Provide the fraction done to the BOINC client, 
         // this is necessary for the percentage bar on the client
         boinc_fraction_done(fraction_done);

         // Check the status of the client if not in standalone mode     
         process_status = check_boinc_status(handleProcess,process_status);
      }

      // Check the status of the child process    
      process_status = check_child_status(handleProcess,process_status);
    }

    //----- End of main loop ---------------------------------------------------------------------------	


    // Time delay to ensure model files are all flushed to disk
    std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(60));

    // Print content of key model files to help with diagnosing problems
    print_last_lines("NODE.001_01", 70);    //  main model output log	

    // To check whether model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.
    if (file_exists(ifs_stat)) {
       std::string ifs_word="";
       fread_last_line(ifs_stat, stat_lastline);
       oifs_parse_stat(stat_lastline, ifs_word, 3);
       std::cerr << "Last line of ifs.stat, ifs_word: " << stat_lastline << ", " << ifs_word << '\n';
       if (ifs_word!="CNT0") {
         std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
         // print extra files to help diagnose fail
         print_last_lines("ifs.stat",8);
         print_last_lines("rcf",11);              // openifs restart control
         print_last_lines("waminfo",17);          // wave model restart control
         print_last_lines(progress_file,8);
         std::cerr << "..Failed, model did not complete successfully" << std::endl;
         return 1;
       }
    }
    // ifs.stat has not been produced, then model did not start
    else {
       std::cerr << "..Failed, model did not start" << std::endl;
       return 1;	    
    }

    // Update model_completed
    model_completed = 1;

    // Handle the last ICM files
    second_part = get_second_part(last_iter, exptid);

    // Move the ICMGG, ICMSH and ICMUA model output files to the task folder in the project directory
    std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
    for (const auto& part : icm) {
       retval = move_result_file(slot_path, temp_path, part, second_part);    // GC. TODO: combine part with second_part and pass as single argument
       if (retval) {
          std::cerr << "..Copying " << part << " result file to the temp folder in the projects directory failed" << "\n";
          return retval;
       }
    }

    boinc_begin_critical_section();

    //-----------------------------Create the final results zip file-----------------------------------------

    zfl.clear();
    std::string node_file = slot_path + "/NODE.001_01";
    zfl.push_back(node_file);
    std::string ifsstat_file = slot_path + "/ifs.stat";
    zfl.push_back(ifsstat_file);
    std::cerr << "Adding to the zip: " << node_file << '\n';
    std::cerr << "Adding to the zip: " << ifsstat_file << '\n';

    // Read the remaining list of files from the slots directory and add the matching files to the list of files for the zip
    // GC. TODO. Update to C++ 17.
    DIR *dirp = opendir(temp_path.c_str());
    if (dirp)
    {
      regex_t regex;
      regcomp(&regex,"\\+",0);

      struct dirent *dir;
      while ((dir = readdir(dirp)) != NULL) {
         //std::cerr << "In temp folder: "<< dir->d_name << '\n';

         if (!regexec(&regex,dir->d_name,(size_t) 0,NULL,0)) {
            zfl.push_back(temp_path + "/" + dir->d_name);
            std::cerr << "Adding to the zip: " << (temp_path+"/" + dir->d_name) << '\n';
         }
      }
      regfree(&regex);
      closedir(dirp);
    }

    // If running under a BOINC client
    if (!standalone) {
       if (zfl.size() > 0){

          // Create the zipped upload file from the list of files added to zfl
          std::string upload_file = project_path + result_base_name + "_" + std::to_string(upload_file_number) + ".zip";

          std::cerr << "Compressing final upload file: " << upload_file << '\n';

          // Time the compression for diagnostics
          auto start = chrono::high_resolution_clock::now();
          auto outcome = cpdn_zip(upload_file, zfl);
          auto stop = chrono::high_resolution_clock::now();
          auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
          std::cerr << "Time taken to compress final upload file: " << duration.count() << " ms\n";
          
          retval = outcome ? 0 : 1;

          if (retval) {
             std::cerr << "..compressing final upload file failed" << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (auto j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                try {
                    fs::remove(zfl[j]);
                } catch (fs::filesystem_error& e) {
                    std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                }
             }
          }

          // Upload the file. In BOINC the upload file is the logical name, not the physical name
          std::string upload_file_name = "upload_file_" + std::to_string(upload_file_number) + ".zip";
          std::cerr << "Uploading the final file: " << upload_file_name << '\n';
          std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(20));
          retval = boinc_upload_file(upload_file_name);
          if (retval) {
             std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          retval = boinc_upload_status(upload_file_name);
          if (!retval) {
             std::cerr << "Finished the upload of the final file" << '\n';
          }

	       // Produce trickle
          process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
       }
       boinc_end_critical_section();
    }

    // Else running in standalone
    else {
       std::string upload_file_name = app_name + "_" + unique_member_id + "_" + start_date + "_" + \
                                      std::to_string(num_days_trunc) + "_" + batchid + "_" + wuid + "_" + \
                                      std::to_string(upload_file_number) + ".zip";
       std::cerr << "The final upload_file_name is: " << upload_file_name << '\n';

       // Create the zipped upload file from the list of files added to zfl
       std::string upload_file = project_path + upload_file_name;

       if (zfl.size() > 0) {
          if (!cpdn_zip(upload_file, zfl)) {
             retval = 1;
          }
          if (retval) {
             std::cerr << "..Creating the compressed upload file failed" << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (auto j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                try {
                  fs::remove(zfl[j]);
                } catch (const fs::filesystem_error& e) {
                  std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                }
             }
         }
         // Produce trickle
         process_trickle(current_cpu_time,wu_name,result_base_name,slot_path,current_iter,standalone);
       }
    }

    //-------------------------------------------------------------------------------------------------------

    // Now that the task has finished, remove the temp folder
    fs::remove_all(temp_path);

    boinc_end_critical_section();

    // Delay to ensure all files are flushed to disk before exiting
    std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(120));
    std::cerr << "Task finished." << std::endl;

    // if finished normally
    if (process_status == 1){
      boinc_finish(0);     // boinc_finish() exits, no further code executed after this call.
      return 0;
    }
    else if (process_status == 2){
      boinc_finish(0);
      return 0;
    }
    else {
      boinc_finish(1);
      return 1;
    }	
}
