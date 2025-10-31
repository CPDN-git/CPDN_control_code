//
// Control code for the WRF application in the climateprediction.net project
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include "cpdn_control.h"

int main(int argc, char** argv) {
    std::string ifsdata_file, ic_ancil_file, climate_data_file, horiz_resolution, vert_resolution, grid_type;
    std::string project_path, tmpstr1, tmpstr2, tmpstr3;
    std::string ifs_line="", iter="0", ifs_word="", second_part, upload_file_name, last_line="";
    std::string upfile(""), resolved_name, upload_file, result_base_name;
    std::string wu_name="", project_dir="", version="";
    int upload_interval, timestep_interval, ICM_file_interval, retval=0, i, j;
    int process_status=1, restart_interval, current_iter=0, count=0, trickle_upload_count;
    int last_cpu_time, restart_cpu_time = 0, upload_file_number, model_completed, restart_iter;
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


    // Initialise BOINC
    retval = initialise_boinc(wu_name, project_dir, version);
    if (retval) {
       cerr << "..BOINC initialisation failed" << "\n";
       return retval;
    }

    cerr << "(argv0) " << argv[0] << "\n";
    cerr << "(argv1) start_date: " << argv[1] << "\n";
    cerr << "(argv2) exptid: " << argv[2] << "\n";
    cerr << "(argv3) unique_member_id: " << argv[3] << "\n";
    cerr << "(argv4) batchid: " << argv[4] << "\n";
    cerr << "(argv5) wuid: " << argv[5] << "\n";
    cerr << "(argv6) fclen: " << argv[6] << "\n";
    cerr << "(argv7) app_name: " << argv[7] << "\n";
    cerr << "(argv8) nthreads: " << argv[8] << "\n";

    // Read the exptid, batchid, version, wuid from the command line
    std::string start_date = argv[1]; // simulation start date
    std::string exptid = argv[2];     // experiment id
    std::string unique_member_id = argv[3];  // umid
    std::string batchid = argv[4];    // batch id
    std::string wuid = argv[5];       // workunit id
    std::string fclen = argv[6];      // number of simulation days
    std::string app_name = argv[7];   // CPDN app name
    std::string nthreads = argv[8];   // number of OPENMP threads

    double num_days = atof(fclen.c_str()); // number of simulation days
    int num_days_trunc = (int) num_days; // number of simulation days truncated to an integer
    
    // Get the slots path (the current working path)
    std::string slot_path = std::filesystem::current_path();
    if (slot_path == "") {
      cerr << "..current_path() returned empty" << "\n";
    }
    else {
      cerr << "Working directory is: "<< slot_path << "\n";      
    }

    if (!boinc_is_standalone()) {

      // Get the project path
      project_path = project_dir + std::string("/");
      cerr << "Project directory is: " << project_path << "\n";
	    
      // Get the app version and re-parse to add a dot
      version = std::to_string(dataBOINC.app_version);
      if (version.length()==2) {
         version = version.insert(0,".");
         //cerr << "version: " << version << "\n";
      }
      else if (version.length()==3) {
         version = version.insert(1,".");
         //cerr << "version: " << version << "\n";
      }
      else if (version.length()==4) {
         version = version.insert(2,".");
         //cerr << "version: " << version << "\n";
      }
      else {
         cerr << "..Error with the length of app_version, length is: " << version.length() << "\n";
         return 1;
      }
	    
      cerr << "app name: " << app_name << "\n";
      cerr << "version: " << version << "\n";
    }
    // Running in standalone
    else {
      cerr << "Running in standalone mode" << "\n";
      // Set the project path
      project_path = slot_path + std::string("/../projects/");
      cerr << "Project directory is: " << project_path << "\n";
	    
      // In standalone get the app version from the command line
      version = argv[9];
      cerr << "app name: " << app_name << "\n"; 
      cerr << "(argv9) app_version: " << argv[9] << "\n"; 
    }

    boinc_begin_critical_section();
    
    // Create temporary folder for moving the results to and uploading the results from
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string temp_path = project_path + app_name + std::string("_") + wuid;
    cerr << "Location of temp folder: " << temp_path << "\n";
    if (mkdir(temp_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) cerr << "..mkdir for temp folder for results failed" << "\n";

    // Move and unzip the app file
    retval = move_and_unzip_app_file(app_name, version, project_path, slot_path);
    if (retval) {
       cerr << "..move_and_unzip_app_file failed" << "\n";
       return retval;
    }

    // Process the Namelist/workunit file:
    std::string namelist_zip = slot_path + std::string("/") + app_name + std::string("_") + unique_member_id + std::string("_") + start_date +\
                      std::string("_") + std::to_string(num_days_trunc) + std::string("_") + batchid + std::string("_") + wuid + std::string(".zip");
		
    // Get the name of the 'jf_' filename from a link within the namelist file
    std::string wu_source = get_tag(namelist_zip);

    // Copy the namelist files to the working directory
    std::string wu_destination = namelist_zip;
    cerr << "Copying the namelist files from: " << wu_source << " to: " << wu_destination << "\n";
    retval = boinc_copy(wu_source.c_str(), wu_destination.c_str());
    if (retval) {
       cerr << "..Copying the namelist files to the working directory failed" << "\n";
       return retval;
    }

    // Unzip the namelist zip file
    cerr << "Unzipping the namelist zip file: " << namelist_zip << "\n";
    retval = boinc_zip(UNZIP_IT, namelist_zip.c_str(), slot_path);
    if (retval) {
       cerr << "..Unzipping the namelist file failed" << "\n";
       return retval;
    }
    // Remove the zip file
    else {
       std::remove(namelist_zip.c_str());
    }


    // Process the ancillary files:
    std::string ancil_zip = slot_path + std::string("/wrf_ancil_") + unique_member_id + std::string("_") + start_date +\
                      std::string("_") + fclen + std::string("_") + batchid + std::string("_") + wuid + std::string(".zip");
    // Get the name of the 'jf_' filename from a link within the ANCIL_FILE
    std::string ancil_target = get_tag(ancil_zip);
    // Copy the ancils to working directory
    std::string ancil_destination = ancil_zip;
    cerr << "Copying the ancils from: " << ancil_target << " to: " << ancil_destination << "\n";
    retval = boinc_copy(ancil_target.c_str(),ancil_destination.c_str());
    if (retval) {
       cerr << "..Copying the ancils to the working directory failed" << "\n";
       return retval;
    }

    // Unzip the ancils zip file
    cerr << "Unzipping the ancils zip file: " << ancil_zip << "\n";
    retval = boinc_zip(UNZIP_IT,ancil_zip.c_str(),slot_path);
    if (retval) {
       cerr << "..Unzipping the ancils file failed" << "\n";
       return retval;
    }
    // Remove the zip file
    else {
       std::remove(ancil_zip.c_str());
    }


    // Define the name and location of the progress file
    std::string progress_file = slot_path+std::string("/progress_file_")+wuid+std::string(".xml");
	
    // Model progress is held in the progress file
    // First check if a file is not already present from an unscheduled shutdown
    cerr << "Checking for progress XML file: " << progress_file << "\n";

    if ( file_exists(progress_file) && !file_is_empty(progress_file) ) {
       std::ifstream progress_file_in(progress_file);
       std::stringstream progress_file_buffer;
       xml_document<> doc;

       // If present parse file and extract values
       progress_file_in.open(progress_file);
       cerr << "Opened progress file ok : " << progress_file << "\n";
       progress_file_buffer << progress_file_in.rdbuf();
       progress_file_in.close();
	    
       // Parse XML progress file
       // RapidXML needs careful memory management. Use string to preserve memory for later xml_node calls.
       // Passing &progress_file_buffer.str()[0] caused new str on heap & memory error.
       std::string prog_contents = progress_file_buffer.str();       // could use vector<char> here

       doc.parse<0>(&prog_contents[0]);
       xml_node<> *root_node = doc.first_node("running_values");
       xml_node<> *last_cpu_time_node = root_node->first_node("last_cpu_time");
       xml_node<> *upload_file_number_node = root_node->first_node("upload_file_number");
       xml_node<> *last_iter_node = root_node->first_node("last_iter");
       xml_node<> *last_upload_node = root_node->first_node("last_upload");
       xml_node<> *model_completed_node = root_node->first_node("model_completed");

       // Set the values from the XML
       last_cpu_time = std::stoi(last_cpu_time_node->value());
       upload_file_number = std::stoi(upload_file_number_node->value());
       last_iter = last_iter_node->value();
       last_upload = std::stoi(last_upload_node->value());
       model_completed = std::stoi(model_completed_node->value());

       // Adjust last_iter to the step of the previous model restart dump step.
       // This is always a multiple of the restart frequency

       cerr << "-- Model is restarting --\n";
       cerr << "Adjusting last_iter, " << last_iter << ", to previous model restart step.\n";
       restart_iter = stoi(last_iter);
       restart_iter = restart_iter - ((restart_iter % restart_interval) - 1);   // -1 because the model will continue from restart_iter.
       last_iter = to_string(restart_iter); 
    }
    else {
       // Set the initial values for start of model run
       last_cpu_time = 0;
       upload_file_number = 0;
       last_iter = "0";
       last_upload = 0;
       model_completed = 0;
    }
	    	    
    // Update progress file with current values
    update_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

    fraction_done = 0;
    trickle_upload_count = 0;
    
    // Get result_base_name to construct upload file names using 
    // the first upload as an example and then stripping off '_0.zip'
    if (!boinc_is_standalone()) {
       retval = boinc_resolve_filename_s("upload_file_0.zip", resolved_name);
       if (retval) {
          cerr << "..boinc_resolve_filename failed" << "\n";
       }

       cerr << "resolved_name: " << resolved_name << "\n";

//GC recoded this as it's potentially hazardous & could cause memory errors (& strip_path() has been removed from control_code.cpp)
//   1. C++ standard forbids c_str() to be cast to non-const char* in order to modify the string's internal buffer.
//      (modifying internal buffer of a string is undefined behaviour)
//   2. strncpy() does not null-terminate if the source string is longer than the specified length
//   3. -6 is applied without testing the length of the string and could cause a negative length and overwrite memory.
//       strncpy(const_cast<char*> (result_base_name.c_str()), strip_path(resolved_name.c_str()), strlen(strip_path(resolved_name.c_str()))-6);
       std::filesystem::path p_resolved(resolved_name);
       std::string      base_name = p_resolved.filename().string();
       constexpr auto   res_len = strlen("_0.zip");

       auto result_sub_len = (base_name.length() > res_len) ? base_name.length() - res_len : 0;
       result_base_name = base_name.substr(0, result_sub_len);
//GC end of recode

       cerr << "result_base_name: " << result_base_name << "\n";
       if (strcmp(result_base_name.c_str(), "upload_file")==0) {
          cerr << "..Failed to get result name" << "\n";
          return 1;
       }
    }
    
    
    
    // Start part 1 of the WRF job
    std::string strCmd = slot_path + std::string("/real.exe");
    handleProcess = launch_process_wrf(slot_path, strCmd.c_str());
    if (handleProcess > 0) process_status = 0;

    boinc_end_critical_section();

    int total_count = 0;

    // process_status = 0 running
    // process_status = 1 stopped normally
    // process_status = 2 stopped with quit request from BOINC
    // process_status = 3 stopped with child process being killed
    // process_status = 4 stopped with child process being stopped


    // Periodically check the process status and the BOINC client status
    while (process_status == 0) {
       sleep_until(system_clock::now() + seconds(1));

       last_iter = iter;
       count = 0;

       // Update progress file with current values
       update_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

       // Calculate current_cpu_time, only update if cpu_time returns a value
       if (cpu_time(handleProcess)) {
          current_cpu_time = last_cpu_time + cpu_time(handleProcess);
          //fprintf(stderr,"current_cpu_time: %1.5f\n",current_cpu_time);
       }
       
       // Calculate the fraction done
       fraction_done = model_frac_done( atof(iter.c_str()), total_nsteps, atoi(nthreads.c_str()) );
       //fprintf(stderr,"fraction done: %.6f\n", fraction_done);
     

       if (!boinc_is_standalone()) {
	 // If the current iteration is at a restart iteration     
	 if (!(std::stoi(iter)%restart_interval)) restart_cpu_time = current_cpu_time;
	      
         // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
         boinc_report_app_status(current_cpu_time,restart_cpu_time,fraction_done);

         // Provide the fraction done to the BOINC client, 
         // this is necessary for the percentage bar on the client
         boinc_fraction_done(fraction_done);
	  
         // Check the status of the client if not in standalone mode     
         process_status = check_boinc_status(handleProcess,process_status);
       }
	
       // Check the status of the child process    
       process_status = check_child_status(handleProcess,process_status);
    }
    
    
    
    // If first part successful start part 2 of the WRF job
    if (process_status) {
       std::string strCmd = slot_path + std::string("/wrf.exe");
       handleProcess = launch_process_wrf(slot_path, strCmd.c_str());
       if (handleProcess > 0) process_status = 0;


       // Periodically check the process status and the BOINC client status
       while (process_status == 0) {
          sleep_until(system_clock::now() + seconds(1));

          // Update progress file with current values
          update_progress_file(progress_file, last_cpu_time, upload_file_number, last_iter, last_upload, model_completed);

          // Calculate current_cpu_time, only update if cpu_time returns a value
          if (cpu_time(handleProcess)) {
            current_cpu_time = last_cpu_time + cpu_time(handleProcess);
            //fprintf(stderr,"current_cpu_time: %1.5f\n",current_cpu_time);
          }
       
          // Calculate the fraction done
          fraction_done = model_frac_done( atof(iter.c_str()), total_nsteps, atoi(nthreads.c_str()) );
          //fprintf(stderr,"fraction done: %.6f\n", fraction_done);
     

          if (!boinc_is_standalone()) { 
	    // If the current iteration is at a restart iteration     
	    if (!(std::stoi(iter)%restart_interval)) restart_cpu_time = current_cpu_time;
	      
            // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
            boinc_report_app_status(current_cpu_time,restart_cpu_time,fraction_done);

            // Provide the fraction done to the BOINC client, 
            // this is necessary for the percentage bar on the client
            boinc_fraction_done(fraction_done);
	  
            // Check the status of the client if not in standalone mode     
            process_status = check_boinc_status(handleProcess,process_status);
          }
	
          // Check the status of the child process    
          process_status = check_child_status(handleProcess,process_status);
       }
    }
    
    
    // Time delay to ensure model files are all flushed to disk
    sleep_until(system_clock::now() + seconds(60));    
    
    
    boinc_begin_critical_section();

    // Create the final results zip file

    zfl.clear();
    std::string freezeH2O_file = slot_path + std::string("/freezeH2O.dat");
    zfl.push_back(freezeH2O_file);
    std::string qr_acr_qs_file = slot_path + std::string("/qr_acr_qs.dat");
    zfl.push_back(qr_acr_qs_file);
    std::string qr_acr_qg_file = slot_path + std::string("/qr_acr_qg.dat");
    zfl.push_back(qr_acr_qg_file);
    std::string namelist_output_file = slot_path + std::string("/namelist.output");
    zfl.push_back(namelist_output_file);
    std::string wrfbdy_d01_file = slot_path + std::string("/wrfbdy_d01");
    zfl.push_back(wrfbdy_d01_file);
    std::string wrflowinp_d01_file = slot_path + std::string("/wrflowinp_d01");
    zfl.push_back(wrflowinp_d01_file);
    std::string wrfinput_d01_file = slot_path + std::string("/wrfinput_d01");
    zfl.push_back(wrfinput_d01_file);
    
    
    // Read the remaining list of files from the slots directory and add the matching files to the list of files for the zip
    dirp = opendir(slot_path.c_str());
    if (dirp) {
        regcomp(&regex,"^[wrfout_d+]",0);
        while ((dir = readdir(dirp)) != NULL) {
          //fprintf(stderr,"In slots folder: %s\n",dir->d_name);

          if (!regexec(&regex,dir->d_name,(size_t) 0,NULL,0)) {
            zfl.push_back(slot_path+std::string("/")+dir->d_name);
            cerr << "Adding to the zip: " << (slot_path+std::string("/") + dir->d_name) << '\n';
          }
        }
        regfree(&regex);
        closedir(dirp);
    }

    // If running under a BOINC client
    if (!boinc_is_standalone()) {
       if (zfl.size() > 0){

          // Create the zipped upload file from the list of files added to zfl
          upload_file = project_path + result_base_name + "_" + std::to_string(upload_file_number) + ".zip"; 

          cerr << "Zipping up the final file: " << upload_file << "\n";
          upfile = upload_file;
          retval = boinc_zip(ZIP_IT, upfile, &zfl);
          upfile.clear();

          if (retval) {
             cerr << "..Zipping up the final file failed" << "\n";
             boinc_end_critical_section();
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
          cerr << "Uploading the final file: " << upload_file_name << "\n";
          sleep_until(system_clock::now() + seconds(20));
          boinc_upload_file(upload_file_name);          
          retval = boinc_upload_status(upload_file_name);
          if (retval) {
             cerr << "Finished the upload of the final file" << "\n";
          }
	       
	  // Produce trickle
          process_trickle(current_cpu_time,wu_name.c_str(),result_base_name,slot_path,current_iter);
       }
       boinc_end_critical_section();
    }
    // Else running in standalone
    else {
       upload_file_name = app_name + unique_member_id + std::string("_") + start_date + std::string("_") + \
                          fclen + std::string("_") + batchid + std::string("_") + wuid + std::string("_") + \
                          to_string(upload_file_number) + std::string(".zip");
       cerr << "The final upload_file_name is: " << upload_file_name << "\n";

       // Create the zipped upload file from the list of files added to zfl
       upload_file = project_path + upload_file_name;

       if (zfl.size() > 0){
          upfile = upload_file;
          retval = boinc_zip(ZIP_IT,upfile,&zfl);
          upfile.clear();
          if (retval) {
             cerr << "..Creating the zipped upload file failed" << "\n";
             boinc_end_critical_section();
             return retval;
           }
        }
	// Produce trickle
        process_trickle(current_cpu_time,wu_name.c_str(),result_base_name,slot_path,current_iter);     
    }

    // Now task has finished, remove the temp folder
    std::remove(temp_path.c_str());

    sleep_until(system_clock::now() + seconds(120));
    
    // if finished normally
    if (process_status == 1){
      boinc_end_critical_section();
      boinc_finish(0);
      cerr << "Task finished" << endl;
      return 0;
    }
    else if (process_status == 2){
      boinc_end_critical_section();
      boinc_finish(0);
      cerr << "Task finished" << endl;
      return 0;
    }
    else {
      boinc_end_critical_section();
      boinc_finish(1);
      cerr << "Task finished" << endl;
      return 1;
    }	
}
