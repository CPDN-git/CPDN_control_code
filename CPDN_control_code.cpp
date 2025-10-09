//
// Control code functions for the climateprediction.net project (CPDN)
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include <iomanip>
#include "CPDN_control_code.h"

// Initialise BOINC and set the options
int initialise_boinc(std::string& wu_name, std::string& project_dir, std::string& version, int& standalone) {

    //boinc_init_diagnostics(BOINC_DIAG_DEFAULTS);
    boinc_init();
    boinc_parse_init_data_file();

    // Get BOINC user preferences
    APP_INIT_DATA dataBOINC;
    boinc_get_init_data(dataBOINC);
    
    wu_name = dataBOINC.wu_name;
    project_dir = dataBOINC.project_dir;
    version = std::to_string(dataBOINC.app_version);

    //cerr << "wu_name: " << wu_name << '\n';
    //cerr << "project_dir: " << project_dir << '\n';
    //cerr << "version: " << version << '\n';

    // Set BOINC optional values
    BOINC_OPTIONS options;
    boinc_options_defaults(options);
    options.main_program = true;
    options.multi_process = true;
    options.check_heartbeat = true;
    options.handle_process_control = true;  // the control code will handle all suspend/quit/resume
    options.direct_process_action = false;  // the control won't get suspended/killed by BOINC
    options.send_status_msgs = false;

    // Check whether BOINC is running in standalone mode
    standalone = boinc_is_standalone();
    
    return boinc_init_options(&options);
}


// GC. recoded from original. do not use putenv, it stores the pointer of memory passed in (see multiple stackexchange posts on this issue)
bool set_env_var(const std::string& name, const std::string& val) {
    return (setenv(name.c_str(), val.c_str(), 1) == 0);     // 1 = overwrite existing value, true on success.
}


// Move and unzip the app file
int move_and_unzip_app_file(std::string app_name, std::string version, std::string project_path, std::string slot_path) {
   // GC. TODO. This code could be combined with copy_and_unzip() to avoid code duplication.

    int retval = 0;

    // macOS
    #if defined (__APPLE__)
       std::string app_file = app_name + "_app_" + version + "_x86_64-apple-darwin.zip";
    // ARM
    #elif defined (_ARM) 
       std::string app_file = app_name + "_app_" + version + "_aarch64-poky-linux.zip";
    // Linux
    #else
       std::string app_file = app_name + "_app_" + version + "_x86_64-pc-linux-gnu.zip";
    #endif

    // Copy the app file to the working directory
    std::filesystem::path app_source = project_path;
    app_source /= app_file;
    std::filesystem::path app_destination = slot_path;
    app_destination /= app_file;
    cerr << "Copying: " << app_source << " to: " << app_destination << "\n";

    // GC. Replace boinc copy with modern C++17 filesystem copy.  Overwrite to match boinc_copy behaviour.
    try {
      std::filesystem::copy_file(app_source, app_destination, std::filesystem::copy_options::overwrite_existing);
    } 
    catch (const std::filesystem::filesystem_error& e) {
      cerr << "..move_and_unzip_app: Error copying file: " << app_source << " to: " << app_destination << ", error: " << e.what() << "\n";
      return 1;
    }

    // Unzip the app zip file
    std::filesystem::path app_zip_path = slot_path;
    app_zip_path /= app_file;
    cerr << "Unzipping the app zip file: " << app_zip_path << "\n";

    if (!cpdn_unzip(app_zip_path, slot_path)){
       retval = 1;               // Or some other non-zero error code
       cerr << "..Unzipping the app file failed" << "\n";
       return retval;
    }
    else {
       try {
           std::filesystem::remove(app_zip_path);
       } catch (const std::filesystem::filesystem_error& e) {
           cerr << "..move_and_unzip_app_file(). Error removing file: " << app_zip_path << ", error: " << e.what() << "\n";
       }
    }
    return retval;
}


const char* strip_path(const char* path) {
    int jj;
    for (jj = (int) strlen(path);
    jj > 0 && path[jj-1] != '/' && path[jj-1] != '\\'; jj--);
    return (const char*) path+jj;
}


int check_child_status(long handleProcess, int process_status) {
    int stat,pid;

    // Check whether child processed has exited
    // waitpid will return process id of zombie (finished) process; zero if still running
    if ( (pid=waitpid(handleProcess,&stat,WNOHANG)) > 0 ) {
       process_status = 1;
       // Child exited normally but model might still have failed
       if (WIFEXITED(stat)) {
          process_status = 1;
          cerr << "..The child process terminated with status: " << WEXITSTATUS(stat) << '\n';
       }
       // Child process has exited due to signal that was not caught
       // n.b. OpenIFS has its own signal handler.
       else if (WIFSIGNALED(stat)) {
          process_status = 3;
          cerr << "..The child process has been killed with signal: " << WTERMSIG(stat) << '\n';
       }
       // Child is stopped
       else if (WIFSTOPPED(stat)) {
          process_status = 4;
          cerr << "..The child process has stopped with signal: " << WSTOPSIG(stat) << '\n';
       }
    }
    else if ( pid == -1) {
      // should not get here, it means the child could not be found
      process_status = 5;
      cerr << "..Unable to retrieve status of child process " << '\n';
      perror("waitpid() error");
    }
    return process_status;
}


int check_boinc_status(long handleProcess, int process_status) {
    BOINC_STATUS status;
    boinc_get_status(&status);

    // If a quit, abort or no heartbeat has been received from the BOINC client, end child process
    if (status.quit_request) {
       cerr << "Quit request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 2;
       return process_status;
    }
    else if (status.abort_request) {
       cerr << "Abort request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    else if (status.no_heartbeat) {
       cerr << "No heartbeat received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    // Else if BOINC client is suspended, suspend child process and periodically check BOINC client status
    else {
       if (status.suspended) {
          cerr << "Suspend request received from the BOINC client, suspending the child process" << '\n';
          kill(handleProcess, SIGSTOP);

          while (status.suspended) {
             boinc_get_status(&status);
             if (status.quit_request) {
                cerr << "Quit request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 2;
                return process_status;
             }
             else if (status.abort_request) {
                cerr << "Abort request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             else if (status.no_heartbeat) {
                cerr << "No heartbeat received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             sleep_until(system_clock::now() + seconds(1));
          }
          // Resume child process
          cerr << "Resuming the child process" << "\n";
          kill(handleProcess, SIGCONT);
          process_status = 0;
       }
       return process_status;
    }
}


long launch_process_oifs(const std::string slot_path, const std::string strCmd, const std::string exptid, const std::string app_name) {
    int retval = 0;
    long handleProcess;

    switch((handleProcess=fork())) {
       case -1: {
          cerr << "..Unable to start a new child process" << "\n";
          exit(0);
          break;
       }
       case 0: { //The child process

          // Set the GRIB_SAMPLES_PATH environmental variable
          std::string GRIB_SAMPLES_var = slot_path + "/eccodes/ifs_samples/grib1_mlgrib2";
          if ( !set_env_var("GRIB_SAMPLES_PATH", GRIB_SAMPLES_var) )  {
            cerr << "..Setting the GRIB_SAMPLES_PATH failed" << std::endl;
          }
          cerr << "The GRIB_SAMPLES_PATH environmental variable is: " << getenv("GRIB_SAMPLES_PATH") << "\n";

          // Set the GRIB_DEFINITION_PATH environmental variable
          std::string GRIB_DEF_var = slot_path + "/eccodes/definitions";
          if ( !set_env_var("GRIB_DEFINITION_PATH", GRIB_DEF_var) )  {
            cerr << "..Setting the GRIB_DEFINITION_PATH failed" << "\n";
          }
          cerr << "The GRIB_DEFINITION_PATH environmental variable is: " << getenv("GRIB_DEFINITION_PATH") << "\n";

          if( (app_name == "openifs") || (app_name == "oifs_40r1")) { // OpenIFS 40r1
            cerr << "Executing the command: " << strCmd << " -e " << exptid << "\n";
            retval = execl(strCmd.c_str(),strCmd.c_str(),"-e",exptid.c_str(),NULL);
          }
          else {  // OpenIFS 43r3 and above
            cerr << "Executing the command: " << strCmd << "\n";
            retval = execl(strCmd.c_str(),strCmd.c_str(),NULL,NULL,NULL);
          }

          // If execl returns then there was an error
          cerr << "..The execl() command failed slot_path=" << slot_path << ",strCmd=" << strCmd << ",exptid=" << exptid << "\n";
          exit(retval);
          break;
       }
       default: 
          cerr << "The child process has been launched with process id: " << handleProcess << "\n";
    }
    return handleProcess;
}


long launch_process_wrf(const std::string slot_path, const char* strCmd) {
    int retval = 0;
    long handleProcess;

    switch((handleProcess=fork())) {
       case -1: {
          cerr << "..Unable to start a new child process" << "\n";
          exit(0);
          break;
       }
       case 0: { //The child process
          cerr << "Executing the command: " << strCmd << "\n";
          retval = execl(strCmd,strCmd,NULL,NULL,NULL);

          // If execl returns then there was an error
          if (retval) {
             cerr << "..The execl() command failed slot_path=" << slot_path << ",strCmd=" << strCmd << "\n";
             exit(retval);
             break;
          }
       }
       default: 
          cerr << "The child process has been launched with process id: " << handleProcess << "\n";
    }
    return handleProcess;
}


// Open a file and return the string contained between the arrow tags
std::string get_tag(const std::string &filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
       std::string line;
       while (getline(file, line)) {
          std::string::size_type start = line.find('>');
          if (start != line.npos) {
             std::string::size_type end = line.find('<', start + 1);
             if (end != line.npos) {
                ++start;
                std::string::size_type count_size = end - start;
                return line.substr(start, count_size);
             }
          }
          return "";
       }
       file.close();
    }
    return "";
}


// Read the progress file
void read_progress_file(std::string progress_file, int& last_cpu_time, int& upload_file_number, std::string& last_iter, int& last_upload, int& model_completed) {

    // Parse the progress_file
    std::string progress_file_line = "";
    std::string delimiter = "=";
    std::ifstream progress_file_filestream;
    
    // Open the progress_file file
    if(!(progress_file_filestream.is_open())) {
       progress_file_filestream.open(progress_file);
    } 
    
    // Read the namelist file
    while(std::getline(progress_file_filestream, progress_file_line)) { //get 1 row as a string
       std::istringstream pfs(progress_file_line);   //put line into stringstream

       if (pfs.str().find("last_cpu_time") != std::string::npos) {
          last_cpu_time = std::stoi(pfs.str().substr(pfs.str().find(delimiter)+1, pfs.str().length()-1));
          //cerr << "last_cpu_time: " << std::to_string(last_cpu_time) << '\n';
       }
       else if (pfs.str().find("upload_file_number") != std::string::npos) {
          upload_file_number = std::stoi(pfs.str().substr(pfs.str().find(delimiter)+1, pfs.str().length()-1));
          //cerr << "upload_file_number: " << std::to_string(upload_file_number) << '\n';
       }
       else if (pfs.str().find("last_iter") != std::string::npos) {
          last_iter = pfs.str().substr(pfs.str().find(delimiter)+1, pfs.str().length()-1);
          //cerr << "last_iter: " << last_iter << '\n';
       }
       else if (pfs.str().find("last_upload") != std::string::npos) {
          last_upload = std::stoi(pfs.str().substr(pfs.str().find(delimiter)+1, pfs.str().length()-1));
          //cerr << "last_upload: " << std::to_string(last_upload) << '\n';
       }
       else if (pfs.str().find("model_completed") != std::string::npos) {
          model_completed = std::stoi(pfs.str().substr(pfs.str().find(delimiter)+1, pfs.str().length()-1));
          //cerr << "model_completed: " << std::to_string(model_completed) << '\n';
       }
    }
    progress_file_filestream.close();
}


// Update the progress file
void update_progress_file(std::string progress_file, int last_cpu_time, int upload_file_number,
                          std::string last_iter, int last_upload, int model_completed) {

    std::ofstream progress_file_out(progress_file);
    //cerr << "Writing to progress file: " << progress_file << "\n";

    // Write out the new progress file. Note this truncates progress_file to zero bytes if it already exists (as in a model restart)
    progress_file_out << "last_cpu_time=" << std::to_string(last_cpu_time) << '\n';
    progress_file_out << "upload_file_number=" << std::to_string(upload_file_number) << '\n';
    progress_file_out << "last_iter=" << last_iter << '\n';
    progress_file_out << "last_upload=" << std::to_string(last_upload) << '\n';
    progress_file_out << "model_completed="<< std::to_string(model_completed) << std::endl;
    progress_file_out.close();

    //cerr << "last_cpu_time: " << last_cpu_time << "\n";
    //cerr << "upload_file_number: " << upload_file_number << "\n";
    //cerr << "last_iter: " << last_iter << "\n";
    //cerr << "last_upload: " << last_upload << "\n";
    //cerr << "model_completed: " << model_completed << "\n";
}


// Produce the trickle and either upload to the project server or as a physical file
void process_trickle(double current_cpu_time, std::string wu_name, std::string result_base_name, std::string slot_path, int timestep, int standalone) 
{
    //cerr << "current_cpu_time: " << current_cpu_time << "\n";
    //cerr << "wu_name: " << wu_name << "\n";
    //cerr << "result_base_name: " << result_base_name << "\n";
    //cerr << "slot_path: " << slot_path << "\n";
    //cerr << "timestep: " << timestep << "\n";

    std::stringstream trickle_buffer;
    trickle_buffer << "<wu>" << wu_name << "</wu>\n<result>" << result_base_name << "</result>\n<ph></ph>\n<ts>" \
                   << timestep << "</ts>\n<cp>" << current_cpu_time << "</cp>\n<vr></vr>\n";
    std::string trickle = trickle_buffer.str();
    //cerr << "Contents of trickle: \n" << trickle << "\n";

    // Create null terminated, non-const char buffers for the boinc_send_trickle_up call
    // to avoid possible memory faults (as seen in the past).
    std::vector<char> variety{'o','r','i','g','\0'};
    std::vector<char> trickle_data(trickle.begin(), trickle.end());
    trickle_data.push_back('\0');
      
    // Upload the trickle if not in standalone mode
    if (!standalone) {
       cerr << "Uploading trickle at timestep: " << timestep << "\n";
       boinc_send_trickle_up(variety.data(), trickle_data.data());
    }
    else {
       std::stringstream trickle_location_buf;
       trickle_location_buf << slot_path << "/trickle_" << time(NULL) << ".xml" << '\0';
       std::string trickle_location = trickle_location_buf.str();
       cerr << "Writing trickle to location: " << trickle_location << "\n";

       FILE* trickle_file = fopen(trickle_location.c_str(), "w");
       if (trickle_file) {
          std::fwrite(trickle_data.data(), sizeof(trickle_data.data()), 1, trickle_file);
          fclose(trickle_file);
       }
    }
}

// Check whether a file exists
bool file_exists(const std::string& filename) {
    std::ifstream infile(filename.c_str());
    return infile.good();
}

// Check whether file is zero bytes long
// from: https://stackoverflow.com/questions/2390912/checking-for-an-empty-file-in-c
// returns True if file is zero bytes, otherwise False.
bool file_is_empty(const std::string& fpath) {
   return (std::filesystem::file_size(fpath) == 0);
}

// Calculate the cpu_time
double cpu_time(long handleProcess) {
    #ifdef __APPLE_CC__
       double x;
       int retval = boinc_calling_thread_cpu_time(x);
       return x;
    // Placeholder for Windows
    //#elif defined(_WIN32) || defined(_WIN64)
    //   double x;
    //   int retval = boinc_process_cpu_time(GetCurrentProcess(), x);
    //   return x;
    #else
       //getrusage(RUSAGE_SELF,&usage); //Return resource usage measurement
       //auto tv_sec = usage.ru_utime.tv_sec; //Time spent executing in user mode (seconds)
       //auto tv_usec = usage.ru_utime.tv_usec; //Time spent executing in user mode (microseconds)
       //return tv_sec+(tv_usec/1000000); //Convert to seconds
       //fprintf(stderr,"tv_sec: %.5f\n",tv_sec);
       //fprintf(stderr,"tv_usec: %.5f\n",(tv_usec/1000000));
       return linux_cpu_time(handleProcess);
    #endif
}


// returns fraction completed of model run
// (candidate for moving into OpenIFS specific src file)
double model_frac_done(double step, double total_steps, int nthreads ) {
   static int     stepm1 = -1;
   static double  heartbeat = 0.0;
   static bool    debug = false;
   double         frac_done, frac_per_step;
   double         heartbeat_inc;

   frac_done     = step / total_steps;	// this increments slowly, as a model step is ~30sec->2mins cpu
   frac_per_step = 1.0 / total_steps;
   
   if (debug) {
      fprintf( stderr,"get_frac_done: step = %.0f\n", step);
      fprintf( stderr,"        total_steps = %.0f\n", total_steps );
      fprintf( stderr,"      frac_per_step = %f\n",   frac_per_step );
   }
   
   // Constant below represents estimate of how many times around the mainloop
   // before the model completes it's next step. This varies alot depending on model
   // resolution, computer speed, etc. Tune it looking at varied runtimes & resolutions!
   // Higher is better than lower to underestimate.
   // 
   // Impact of speedup due to multiple threads is accounted for below.
   //
   // If we want more accuracy could use the ratio of the model timestep to 1h (T159 tstep) to 
   // provide a 'slowdown' factor for higher resolutions.
   heartbeat_inc = (frac_per_step / (70.0 / (double)nthreads) );

   if ( (int) step > stepm1 ) {
      heartbeat = 0.0;
      stepm1 = (int) step;
   } else {
      heartbeat = heartbeat + heartbeat_inc;
      if ( heartbeat > frac_per_step )  heartbeat = frac_per_step - 0.001;  // slightly less than the next step
      frac_done = frac_done + heartbeat;
   } 

   if (frac_done < 0.0)  frac_done = 0.0;
   if (frac_done > 1.0)  frac_done = 0.9999; // never 100% until wrapper finishes
   if (debug){
      fprintf(stderr, "    heartbeat_inc = %.8f\n", heartbeat_inc);
      fprintf(stderr, "    heartbeat     = %.8f\n", heartbeat );
      double percent = frac_done * 100.0;
      fprintf(stderr, "     percent done = %.3f\n", percent);
   }

   return frac_done;
}

// Construct the second part of the output model filename to be uploaded
// nb. exptid is always 4 characters for OpenIFS.
std::string get_second_part(const std::string& last_iter, const std::string& exptid) {
    std::ostringstream oss;
    oss << exptid << "+" << std::setw(6) << std::setfill('0') << last_iter;
    return oss.str();
}


int move_result_file(std::string slot_path, std::string temp_path, std::string first_part, std::string second_part) {
    int retval = 0;

    // Move result file to the temporary folder in the project directory
    std::string result_file = slot_path + "/" + first_part + second_part;
    std::string temp_file = temp_path + "/" + first_part + second_part;
    //cerr << "Checking for result file: " << result_file << "\n";

    if(file_exists(result_file)) {
       cerr << "Moving result file: " << std::filesystem::path(result_file).filename() << " to projects directory.\n";
       retval = boinc_copy( result_file.c_str(), temp_file.c_str() );

       // If result file has been successfully copied over, remove it from slots directory
       if (!retval) {
          try {
              std::filesystem::remove(result_file);
          } catch (const std::filesystem::filesystem_error& e) {
              cerr << "..move_result_file(). Error removing file: " << result_file << ", error: " << e.what() << "\n";
          }
       }
    }
    return retval;
}


bool check_stoi(std::string& cin) {
    //  check input string is convertable to an integer by checking for any letters
    //  nb. stoi() will convert leading digits if alphanumeric but we know step must be all digits.
    //  Returns true on success, false if non-numeric data in input string.
    //  Glenn Carver

    if (std::any_of(cin.begin(), cin.end(), ::isalpha)) {
        cerr << "..Invalid characters in stoi string: " << cin << "\n";
        return false;
    }

    //  check stoi standard exceptions
    //  n.b. still need to check step <= max_step
    try {
        auto step = std::stoi(cin);
        //cerr << "step converted is : " << step << "\n";
        return true;
    }
    catch (const std::invalid_argument &excep) {
        cerr << "..Invalid input argument for stoi : " << excep.what() << "\n";
        return false;
    }
    catch (const std::out_of_range &excep) {
        cerr << "..Out of range value for stoi : " << excep.what() << "\n";
        return false;
    }
}

bool oifs_parse_stat(const std::string& logline, std::string& stat_column, const int index) {
   //   Parse a line of the OpenIFS ifs.stat log file.
   //      logline  : incoming ifs.stat logfile line to be parsed
   //      stat_col : returned string given by position 'index'
   //  Returns false if string is empty.

   istringstream tokens;
   std::string statstr="";

   //  split input, get token specified by 'column' unless file is corrupted
   tokens.str(logline);
   for (int i=0; i<index; ++i)
      tokens >> statstr;

   if ( statstr.empty() ){
      cerr << "..oifs_parse_stat: warning, statstr is empty: " << logline << '\n';
      return false;
   } else {
      stat_column = statstr;
      return true;
   }
}


bool fread_last_line(const std::string& fname, std::string& logline) {
    //  Function to read & return last line of a file.
    //  Works in a similar way to 'tail -f' command.
    //  fname   : name of file to read
    //  logline : last line read from file, preserved between calls to this fn.
    //  Returns true if a new line read. returns false and logline unchanged
    //          if no new line read, returns false and empty logline if file does not exist.
    //
    //    Glenn carver

    static std::streamoff last_offset = 0;
    static std::string    last_line;
    std::string           line;

    // Check file exists and non-empty
    std::ifstream logfile(fname, std::ios::in);
    if (!logfile.is_open()) {
        logline.clear();
        last_offset = 0;
        cerr << ".. file_last_line(): warning, " << fname << " does not exist." << std::endl;
        return false;
    }

   // Seek to last offset and read lines to file end
   logfile.seekg(last_offset, std::ios::beg);

   while (std::getline(logfile, line)) {
      last_line = line;
   }
   //cerr << "fread_last_line: last line read: " << last_line << '\n';

   // Update last_offset for next call
   last_offset = logfile.tellg();
   if (last_offset == -1) {
      // At EOF, set to file size
      logfile.clear();                   // must clear stream error before attempting to read again
      logfile.seekg(0, std::ios::end);   // seek backwards to start of file to get size.
      last_offset = logfile.tellg();
    }

    logfile.close();

    if (!last_line.empty()) {
        logline = last_line;
        return true;
    }
    return false;    // no new line read and arg logline unchanged
}


bool oifs_valid_step(std::string& step, int nsteps) {
   //  checks for a valid step count in arg 'step'
   //  Returns :   true if step is valid, otherwise false
   //      Glenn

   // make sure step is valid integer
   if (!check_stoi(step)) {
      cerr << "..oifs_valid_step: Invalid characters in stoi string, unable to convert step to int: " << step << '\n';
      return false;
   } else {
      // check step is in valid range: 0 -> total no. of steps
      if (stoi(step)<0) {
         return false;
      } else if (stoi(step) > nsteps) {
         return false;
      }
   }
   return true;
}


int print_last_lines(string filename, int maxlines) {
   // Opens a file if exists and uses circular buffer to read & print last lines of file to stderr.
   // Returns: zero : either can't open file or file is empty
   //          > 0  : no. of lines in file (may be less than maxlines)
   //  Glenn

   int     count = 0;
   int     start, end;
   string  lines[maxlines];
   ifstream filein(filename);

   if ( filein.is_open() ) {
      while ( getline(filein, lines[count%maxlines]) )
         count++;
   }

   if ( count > 0 ) {
      // find the oldest lines first in the buffer, will not be at start if count > maxlines
      start = count > maxlines ? (count%maxlines) : 0;
      end   = min(maxlines,count);

      cerr << ">>> Printing last " << end << " lines from file: " << filename << '\n';
      for ( int i=0; i<end; i++ ) {
         cerr << lines[ (start+i)%maxlines ] << '\n';
      }
      cerr << "------------------------------------------------" << '\n';
   }

   return count;
}


bool read_rcf_file(std::ifstream& rcf_file, std::string& ctime_value, std::string& cstep_value)
{
    // Read the rcf_file if it exists and extract the CTIME and CSTEP variables
    
    std::string delimiter = "\"";
    std::string rcf_file_line;
    int position = 2;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line )) {

       // Check for CSTEP, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CSTEP", position, cstep_value);

       // Check for CTIME, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CTIME", position, ctime_value);

    }
    //cerr << "rcf file CSTEP: " << cstep_value << '\n';
    //cerr << "rcf file CTIME: " << ctime_value << '\n';

    if (cstep_value == "") {
       cerr << "CSTEP value not present in rcf file" << '\n';
       return false;
    } else if (ctime_value == "") {
       cerr << "CTIME value not present in rcf file" << '\n';
       return false;
    } else {
       return true;
    }
}


bool read_delimited_line(std::string& file_line, std::string delimiter, std::string string_to_find, int position, std::string& returned_value)
{
    // Extracts a value from a delimited position on a line of a file

    size_t pos = 0;
    int count = 0;

    if (file_line.find(string_to_find) != std::string::npos ) {
       // From the file line take the field specified by the position
       while ((pos = file_line.find(delimiter)) != std::string::npos) {
          count = count + 1;
          if (count == position) {  
             returned_value = file_line.substr(0,pos);
             // Remove whitespace
             returned_value.erase( std::remove_if( returned_value.begin(), \
                                   returned_value.end(), ::isspace ), returned_value.end() );
          }
          file_line.erase(0, pos + delimiter.length());
       }
    }

    if ( returned_value != "" ) {
       return true;
    } else {
       return false;
    }
}


// Takes the zip file, checks existence and whether empty and copies it to destination and unzips it
// GC. TODO. Convert this to accept std::filesystem::path args.
int copy_and_unzip(const std::string& zipfile, const std::string& destination, const std::string& unzip_path, const std::string& type) {
    int retval = 0;

    // Check for the existence of the zip file
    if( !file_exists(zipfile) ) {
       cerr << "..The " << type << " zip file does not exist: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Check whether the zip file is empty
    if( file_is_empty(zipfile) ) {
       cerr << "..The " << type << " zip file is empty: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }
		
    // Get the name of the 'jf_' filename from a link within the 'zipfile' file
    std::string source = get_tag(zipfile);

    // Copy and unzip the zip file only if the zip file contains a string between tags
    if ( !source.empty() ) {
       // Copy the 'jf_' to the working directory and rename
       cerr << "Copying the " << type << " files from: " << source << " to: " << destination << '\n';
       try {
          std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
       } 
       catch (const std::filesystem::filesystem_error& e) {
          cerr << "..copy_and_unzip: Error copying file: " << source << " to: " << destination << ", error: " << e.what() << "\n";
          return 1;
       }

       cerr << "Unzipping the " << type << " zip file: " << destination << '\n';
       if (!cpdn_unzip(destination, unzip_path)) {
          retval = 1; // Or some other non-zero error code
          cerr << "..Unzipping the " << type << " file failed" << std::endl;
          return retval;
       }
    }
	 // Success, retval is 0
    return retval;
}


