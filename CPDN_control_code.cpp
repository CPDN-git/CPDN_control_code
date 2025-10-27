//
// Control code functions for the climateprediction.net project (CPDN)
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
// Further development: Glenn Carver, CPDN, 2022->
//

#include <iomanip>
#include "CPDN_control_code.h"
#include "openifs.h"

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


// Next two functions allow the use of an override file to set environment variables for testing
// on live tasks on remote machines.  The file is a simple text file with one variable per line in the format:
// VAR=VALUE  or export VAR='VALUE'  (single or double quotes can be used, or no quotes)
// e.g. export OMP_NUM_THREADS=6

// GC. This function should be in a utility library eventually.
/**
 * @brief Attempts to parse a single line from the override file.
 * * Handles common shell formats like "VAR=VALUE" or "export VAR='VALUE'".
 *
 * @param line The line of text to parse.
 * @param name Output parameter for the variable name.
 * @param value Output parameter for the variable value.
 * @return true if successful, false if the line is empty, a comment, or invalid.
 */
bool parse_export(const std::string& line, std::string& name, std::string& value)
{
    std::string working_line = line;

    // Trim leading whitespace
    working_line.erase(0, working_line.find_first_not_of(" \t\n\r"));
    
    // Ignore comments and empty lines
    if (working_line.empty() || working_line[0] == '#') {
        return false;
    }

    // Strip 'export' keyword if present
    const std::string export_prefix = "export ";
    if (working_line.rfind(export_prefix, 0) == 0) {
        working_line.erase(0, export_prefix.length());
    }

    // Find the '=' delimiter
    auto eq_pos = working_line.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0) {
        return false;
    }

    name = working_line.substr(0, eq_pos);
    value = working_line.substr(eq_pos + 1);

    // Tidy up the value (remove surrounding quotes if present)
    value.erase(0, value.find_first_not_of(" \t\n\r")); // Trim leading whitespace
    value.erase(value.find_last_not_of(" \t\n\r") + 1); // Trim trailing whitespace

    if (value.length() >= 2 && 
        ((value.front() == '"' && value.back() == '"') || 
         (value.front() == '\'' && value.back() == '\''))) 
    {
        // Remove surrounding quotes
        value = value.substr(1, value.length() - 2);
    }
    
    // Tidy up the name (trimming is sufficient)
    name.erase(name.find_last_not_of(" \t\n\r") + 1);
    
    return true;
}


/**
 * @brief Checks for the override file and sets environment variables if found.
 * * 
 * @param project_path The base directory.
 * @param filename The override environment filename.
 * @return true if environment variables were successfully processed, false otherwise.
 */
bool process_env_overrides(const fs::path& override_envs)
{
    if (!fs::exists(override_envs)) {
        // Fail silently to avoid highlighting existence of file
        //std::cerr << "Override file not found: " << override_envs.string() << std::endl;
        return false;
    }

    // debugging only. don't advertise existence of file
    //std::cerr << "Processing environment overrides from: " << override_envs.string() << std::endl;
    
    std::ifstream file(override_envs);
    if (!file.is_open()) {
        // Fail silently
        //std::cerr << "Error: Could not open override file for reading." << std::endl;
        return false;
    }

    std::string line;
    bool success = true;
    while (std::getline(file, line))
    {
        std::string var_name;
        std::string var_value;

        if (parse_export(line, var_name, var_value))
        {
            try {
                set_env_var(var_name, var_value);
                std::cerr << "Overriding env var: " << var_name << " = " << var_value << '\n';
            } 
            catch (const std::exception& e) {
                std::cerr << "Error setting variable: " << e.what() << std::endl;
                success = false;
            }
        }
    }

    return success;
}


// Set executable permissions on a file
// GC. this is a workaround currently as cpdn_unzip does not set unix permissions correctly.
bool set_exec_perms(const std::string& filepath) {
    // 0755 is a standard permission set:
    // Owner: Read, Write, Execute
    // Group: Read, Execute
    // Others: Read, Execute

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    if (chmod(filepath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 ) {
        return false;
    }
#endif

    return true;
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
    fs::path app_source = project_path;
    app_source /= app_file;
    fs::path app_destination = slot_path;
    app_destination /= app_file;
    std::cerr << "Copying: " << app_source << " to: " << app_destination << "\n";

    // GC. Replace boinc copy with modern C++17 filesystem copy.  Overwrite to match boinc_copy behaviour.
    try {
       fs::copy_file(app_source, app_destination,  fs::copy_options::overwrite_existing);
    } 
    catch (const  fs::filesystem_error& e) {
      std::cerr << "..move_and_unzip_app: Error copying file: " << app_source << " to: " << app_destination << ",\nError: " << e.what() << std::endl;
      return 1;
    }

    // Unzip the app zipfile
    std::cerr << "Extracting the app zipfile: " << app_destination << "\n";

    if (!cpdn_unzip(app_destination, slot_path)){
       retval = 1;
       std::cerr << "..Extracting the app zipfile failed" << "\n";
       return retval;
    }
    else {
       try {
            fs::remove(app_destination);
       } catch (const  fs::filesystem_error& e) {
           std::cerr << "..move_and_unzip_app_file(). Error removing file: " << app_destination << ",\nError: " << e.what() << std::endl;
       }
    }
    return retval;
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
          std::cerr << "..The child process terminated with status: " << WEXITSTATUS(stat) << '\n';
       }
       // Child process has exited due to signal that was not caught
       // n.b. OpenIFS has its own signal handler.
       else if (WIFSIGNALED(stat)) {
          process_status = 3;
          std::cerr << "..The child process has been killed with signal: " << WTERMSIG(stat) << '\n';
       }
       // Child is stopped
       else if (WIFSTOPPED(stat)) {
          process_status = 4;
          std::cerr << "..The child process has stopped with signal: " << WSTOPSIG(stat) << '\n';
       }
    }
    else if ( pid == -1) {
      // should not get here, it means the child could not be found
      process_status = 5;
      std::cerr << "..Unable to retrieve status of child process " << '\n';
      perror("waitpid() error");
    }
    return process_status;
}


int check_boinc_status(long handleProcess, int process_status) {
    BOINC_STATUS status;
    boinc_get_status(&status);

    // If a quit, abort or no heartbeat has been received from the BOINC client, end child process
    if (status.quit_request) {
       std::cerr << "Quit request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 2;
       return process_status;
    }
    else if (status.abort_request) {
       std::cerr << "Abort request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    else if (status.no_heartbeat) {
       std::cerr << "No heartbeat received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    // Else if BOINC client is suspended, suspend child process and periodically check BOINC client status
    else {
       if (status.suspended) {
          std::cerr << "Suspend request received from the BOINC client, suspending the child process" << '\n';
          kill(handleProcess, SIGSTOP);

          while (status.suspended) {
             boinc_get_status(&status);
             if (status.quit_request) {
                std::cerr << "Quit request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 2;
                return process_status;
             }
             else if (status.abort_request) {
                std::cerr << "Abort request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             else if (status.no_heartbeat) {
                std::cerr << "No heartbeat received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(1));
          }
          // Resume child process
          std::cerr << "Resuming the child process" << "\n";
          kill(handleProcess, SIGCONT);
          process_status = 0;
       }
       return process_status;
    }
}

// GC. TODO. There does not need to be two separate functions for launching WRF and OpenIFS.
//     This should be combined with more args to handle differences.
// Returns process id on success, -1 on failure.
long launch_process_oifs(const std::string& project_path, const std::string& slot_path, 
                         const std::string& strCmd, const std::string& nthreads,
                         const std::string& exptid, const std::string& app_name)
{
    long handleProcess;

    switch((handleProcess=fork())) {
       case -1: {
          std::cerr << "..Unable to start a new child process" << "\n";
          return -1;      // Don't exit() here, return as this is the parent process.
          break;
       }
       case 0: { // The child process

           // Set the environment variables for the executable.
           if ( !oifs_setenvs(slot_path, nthreads) ) {
             std::cerr << "..Setting the OpenIFS environmental variables failed" << std::endl;
             exit(1);   // Can't continue child process so exit to end child process.
          }

          // --------------------------------------
          // Custom environment variable overrides, if the override file exists.
          // NOTE! This should only be used for testing and never advertised to users.
          {
            fs::path override_env_vars = project_path + "/oifs_override_env_vars";
            process_env_overrides(override_env_vars);
          }
          //---------------------------------------

          // Execute OpenIFS
          // OpenIFS 40r1 requires the -e exptid argument, later versions do not.
          // GC. TODO. This should be an input arg, not decided here.

          if( (app_name == "openifs") || (app_name == "oifs_40r1")) { // OpenIFS 40r1
            std::cerr << "Executing the command: " << strCmd << " -e " << exptid << "\n";
            execl(strCmd.c_str(),strCmd.c_str(),"-e",exptid.c_str(),NULL);
          }
          else {  // OpenIFS 43r3 and above
            std::cerr << "Executing the command: " << strCmd << "\n";
            execl(strCmd.c_str(),strCmd.c_str(),NULL);         // always returns -1 on failure
          }

          // If execl returns there was an error
          int syserr = errno;    // grab the error before any other system call.
          const char* syserr_msg = strerror(syserr);

          std::cerr << "..Launch process failed: execl - errno = " << syserr << ", " << syserr_msg
                     << "\n slot_path=" << slot_path << ",strCmd=" << strCmd << ",exptid=" << exptid << std::endl;

          exit(syserr);  // exit child process with system code for better remote diagnosis.
          break;
       }
       default: 
          std::cerr << "The child process has been launched with process id: " << handleProcess << "\n";
    }
    return handleProcess;
}


long launch_process_wrf(const std::string slot_path, const char* strCmd) {
    int retval = 0;
    long handleProcess;

    switch((handleProcess=fork())) {
       case -1: {
          std::cerr << "..Unable to start a new child process" << std::endl;
          exit(0);
          break;
       }
       case 0: { //The child process
          std::cerr << "Executing the command: " << strCmd << "\n";
          retval = execl(strCmd,strCmd,NULL,NULL,NULL);

          // If execl returns then there was an error
          if (retval) {
             std::cerr << "..The execl() command failed slot_path=" << slot_path << ",strCmd=" << strCmd << std::endl;
             exit(retval);
             break;
          }
       }
       default: 
          std::cerr << "The child process has been launched with process id: " << handleProcess << std::endl;
    }
    return handleProcess;
}



// Open a file and return the "jf_*" string contained between the arrow tags else empty string
// Explanation for Glenn's benefit :).  First run of task the filename contains a 'reference'
// to the real zip file stored in the projects directory.  The reference is in the form of a
// single line e.g. ">../../projects/climateprediction.net/jf_ic_ancil_1234<". 
// This function extracts the strings between the delimiters, so the real zip
// file can be copied to overwrite the file containing the 'jf_' file reference (yes, it's odd and not a good idea).
// On the subsequent runs, what should happen is the client restores the original
// 'reference' (jf_ic_ancil_1234) file. However, some clients do not do this which 
// means the real zip file is there instead. In this case get_tag returns an empty string.

// GC. Modified to avoid reading the entire zip file if it's not a jf_ file reference.
//     Otherwise we end up with a very big string in memory!
std::string get_tag(const std::string &filename) {

    constexpr auto MAX_READ_BYTES = 256;
    std::string buffer(MAX_READ_BYTES, '\0');

    std::ifstream file(filename, std::ios::in);

    if (!file.is_open()) {
         std::cerr << "..get_tag. Failed to open file: " << filename << std::endl;
        return std::string();
    }

    // Read up to MAX_READ_BYTES directly into the string's underlying buffer
    file.read(buffer.data(), MAX_READ_BYTES-1);          // Leave space for final null terminator set above
    
    // Get the actual number of bytes read
    std::streamsize chars_read = file.gcount();

    if (chars_read == 0) {
       return std::string(); // File is empty
    }

    // Check for the "magic number" for zipfiles in case zipfile has already been copied.
    if (chars_read > 2 && buffer[0] == 'P' && buffer[1] == 'K' ) {
       return std::string();
    }

    // Resize string to actual number of chars read
    buffer.resize(chars_read);

    // Look for the delimiters to find the file reference
    // We could still be unlucky here and have a binary file which might just
    // have a > and < in our buffer with garbage in. Negligible risk.
    const char START_TAG = '>';
    const char END_TAG = '<';

    auto start_pos = buffer.find(START_TAG);

    if (start_pos == std::string::npos) {
        return std::string();
    }

    auto tag_start = start_pos + 1;
    auto tag_end   = buffer.find(END_TAG, tag_start);

    if (tag_end == std::string::npos) {
        return std::string();
    }

    // Extract the file reference
    // The length is (position of '<') - (position after '>')
    auto length = tag_end - tag_start;
    
    return buffer.substr(tag_start, length);
}


// Read the progress file
void read_progress_file(std::string progress_file, int& last_cpu_time, int& upload_file_number, 
                        std::string& last_iter, int& last_upload, int& model_completed) {

    // Parse the progress_file
    std::string progress_line = "";
    std::string delimiter = "=";
    std::ifstream progress_filestream;
    
    // Open the progress_file file
    if(!(progress_filestream.is_open())) {
       progress_filestream.open(progress_file);
    } 
    
    // Read the namelist file
    while(std::getline(progress_filestream, progress_line)) { //get 1 row as a string

       if (progress_line.find("last_cpu_time") != std::string::npos) {
          last_cpu_time = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("upload_file_number") != std::string::npos) {
          upload_file_number = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("last_iter") != std::string::npos) {
          last_iter = progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1);
       }
       else if (progress_line.find("last_upload") != std::string::npos) {
          last_upload = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("model_completed") != std::string::npos) {
          model_completed = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
    }
    progress_filestream.close();
}


/**
 * @brief Store task progress in progress_file
 */
void update_progress_file(std::string& progress_file, int last_cpu_time, int upload_file_number,
                          std::string& last_iter, int last_upload, int model_completed)
{
    std::ofstream progress_file_out(progress_file);

    // Write out the new progress file. Note this truncates progress_file to zero bytes if it already exists (as in a model restart)
    // GC Oct/2025. Make progress file a fortran namelist, so the models can easily read it to check the control process is still running.
    progress_file_out << "! CPDN controller progress file & fortran namelist\n"
                      << "&CPDN\n"
                      << "last_cpu_time=" << std::to_string(last_cpu_time) << '\n'
                      << "upload_file_number=" << std::to_string(upload_file_number) << '\n'
                      << "last_iter=" << last_iter << '\n'
                      << "last_upload=" << std::to_string(last_upload) << '\n'
                      << "model_completed="<< std::to_string(model_completed) << '\n'
                      << "/" << std::endl;
    progress_file_out.close();
}


// Produce the trickle and either upload to the project server or as a physical file
void process_trickle(double current_cpu_time, std::string wu_name, std::string result_base_name, std::string slot_path, int timestep, int standalone) 
{
    std::stringstream trickle_buffer;
    trickle_buffer << "<wu>" << wu_name << "</wu>\n<result>" << result_base_name << "</result>\n<ph></ph>\n<ts>" \
                   << timestep << "</ts>\n<cp>" << current_cpu_time << "</cp>\n<vr></vr>\n";
    std::string trickle = trickle_buffer.str();

    // Create null terminated, non-const char buffers for the boinc_send_trickle_up call
    // to avoid possible memory faults (as seen in the past).
    std::vector<char> trickle_data(trickle.begin(), trickle.end());
    trickle_data.push_back('\0');
      
    // Upload the trickle if not in standalone mode
    if (!standalone) {
       std::cerr << "Uploading trickle at timestep: " << timestep << "\n";
       boinc_send_trickle_up( (char*)"orig", trickle_data.data());
    }
    else {
       std::stringstream trickle_location_buf;
       trickle_location_buf << slot_path << "/trickle_" << time(NULL) << ".xml" << '\0';
       std::string trickle_location = trickle_location_buf.str();
       std::cerr << "Writing trickle to location: " << trickle_location << "\n";

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
   return ( fs::file_size(fpath) == 0);
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
    //std::cerr << "Checking for result file: " << result_file << "\n";

    if(file_exists(result_file)) {
       std::cerr << "Moving result file: " <<  fs::path(result_file).filename() << " to projects directory.\n";
       retval = boinc_copy( result_file.c_str(), temp_file.c_str() );

       // If result file has been successfully copied over, remove it from slots directory
       if (!retval) {
          try {
               fs::remove(result_file);
          } catch (const  fs::filesystem_error& e) {
              std::cerr << "..move_result_file(). Error removing file: " << result_file << ", error: " << e.what() << "\n";
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
        std::cerr << "..Invalid characters in stoi string: " << cin << "\n";
        return false;
    }

    //  check stoi standard exceptions
    //  n.b. still need to check step <= max_step
    try {
        std::stoi(cin);
        return true;
    }
    catch (const std::invalid_argument &excep) {
        std::cerr << "..Invalid input argument for stoi : " << excep.what() << "\n";
        return false;
    }
    catch (const std::out_of_range &excep) {
        std::cerr << "..Out of range value for stoi : " << excep.what() << "\n";
        return false;
    }
}

bool oifs_parse_stat(const std::string& logline, std::string& stat_column, const int index) {
   //   Parse a line of the OpenIFS ifs.stat log file.
   //      logline  : incoming ifs.stat logfile line to be parsed
   //      stat_col : returned string given by position 'index'
   //  Returns false if string is empty.

   std::istringstream tokens;
   std::string statstr="";

   //  split input, get token specified by 'column' unless file is corrupted
   tokens.str(logline);
   for (int i=0; i<index; ++i)
      tokens >> statstr;

   if ( statstr.empty() ){
      std::cerr << "..oifs_parse_stat: warning, statstr is empty: " << logline << '\n';
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
        std::cerr << ".. file_last_line(): warning, " << fname << " does not exist." << std::endl;
        return false;
    }

   // Seek to last offset and read lines to file end
   logfile.seekg(last_offset, std::ios::beg);

   while (std::getline(logfile, line)) {
      last_line = line;
   }
   //std::cerr << "fread_last_line: last line read: " << last_line << '\n';

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
      std::cerr << "..oifs_valid_step: Invalid characters in stoi string, unable to convert step to int: " << step << '\n';
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


int print_last_lines(std::string filename, int maxlines) {
   // Opens a file if exists and uses circular buffer to read & print last lines of file to stderr.
   // Returns: zero : either can't open file or file is empty
   //          > 0  : no. of lines in file (may be less than maxlines)
   //  Glenn

   int     count = 0;
   std::string  lines[maxlines];
   std::ifstream filein(filename);

   if ( filein.is_open() ) {
      while ( getline(filein, lines[count%maxlines]) )
         count++;
   }

   if ( count > 0 ) {
      // find the oldest lines first in the buffer, will not be at start if count > maxlines
      int start = count > maxlines ? (count%maxlines) : 0;
      int end   = std::min(maxlines,count);

      std::cerr << ">>> Printing last " << end << " lines from file: " << filename << '\n';
      for ( int i=0; i<end; i++ ) {
         std::cerr << lines[ (start+i)%maxlines ] << '\n';
      }
      std::cerr << "------------------------------------------------" << '\n';
   }

   return count;
}

/**
 * @brief Read the rcf_file line by line and extract CTIME and CSTEP variables.
 *        The input stream rcf_file must be at file start and ctime_value & cstep_value
 *        must be empty strings.
 */
bool read_rcf_file(std::ifstream& rcf_file, std::string& ctime_value, std::string& cstep_value)
{
    std::string delimiter = "\"";
    std::string rcf_file_line;
    int position = 2;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line ))
    {
       // Check for CSTEP, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CSTEP", position, cstep_value);

       // Check for CTIME, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CTIME", position, ctime_value);
    }

    if (cstep_value.empty()) {
       std::cerr << "CSTEP value not present in rcf file" << '\n';
       return false;
    } else if (ctime_value.empty()) {
       std::cerr << "CTIME value not present in rcf file" << '\n';
       return false;
    } else {
       return true;
    }
}


bool read_delimited_line(std::string file_line, const std::string& delimiter, const std::string& to_find, int position, std::string& returned_value)
{
    // Extracts a substring following a positional delimiter if found.

    size_t pos = 0;
    int count = 0;

    if (file_line.find(to_find) != std::string::npos ) {
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
    return !returned_value.empty();
}


/**
 * @brief Searches a line for a specific key and extracts the value substring.
 * 
 * This function is designed to extract the value of an input 'key' in a typical
 * key=value pair contained in the input line.
 * NOTE! It is similar to read_delimited_line but that functions works by a 
 * positional search of a delimiter, rather than expecting a key/value pair.
 * 
 * @param line The input string to search (e.g., the line read from a file).
 * @param key The substring to look for (e.g., "IFSDATA_FILE").
 * @param delimiter The character separating the key from the value (e.g., '=' or ':').
 * @param out_value Reference to a string where the extracted and stripped value will be stored.
 * @return true if the key was found and a value successfully extracted; false otherwise.
 */
bool extract_key_value( const std::string& line, const std::string& key, char delimiter, std::string& out_value ) 
{
    if (line.find(key) == std::string::npos) {
        return false;
    }

    auto pos = line.find(delimiter);
    if (pos == std::string::npos) {
        return false;
    }

    // Extract the substring (the value part)
    // substr(pos + 1) takes the rest of the string after the delimiter.
    out_value = line.substr(pos + 1);

    // Remove space and trailing commas (as in a namelist entry).
    out_value.erase( std::remove(out_value.begin(), out_value.end(), ','), out_value.end() );
    out_value.erase( std::remove(out_value.begin(), out_value.end(), ' '), out_value.end() );

    return true;
}


// Takes the zip file, checks existence and whether empty and copies it to destination and unzips it
// GC. TODO. Convert this to accept  fs::path args.
int copy_and_unzip(const std::string& zipfile, const std::string& destination, const std::string& unzip_path, const std::string& type) {
    int retval = 0;

    // Check for the existence of the zip file
    if( !file_exists(zipfile) ) {
       std::cerr << "..The " << type << " zip file does not exist: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Check whether the zip file is empty
    if( file_is_empty(zipfile) ) {
       std::cerr << "..The " << type << " zip file is empty: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }
		
    // Get the name of the 'jf_' filename from a link within the 'zipfile' file
    std::string source = get_tag(zipfile);

    // Copy and unzip the zip file only if the zip file contains a string between tags.
    // If it doesn't, the real zip file is likely already in the working directory from a previous run.
    if ( !source.empty() ) {
       // Copy the 'jf_' file to the working directory and rename
       if ( file_exists(source) ) {
          std::cerr << "Copying the " << type << " file from: " << source << " to: " << destination << '\n';
          try {
              fs::copy_file(source, destination,  fs::copy_options::overwrite_existing);
          } 
          catch (const  fs::filesystem_error& e) {
             std::cerr << "..copy_and_unzip: Error copying file: " << source << " to: " << destination << ",\nError: " << e.what() << "\n";
             return 1;
          }
       }
       else {
          std::cerr << "..The " << type << " file retrieved from get_tag does not exist: " << source << std::endl;
          return 1;      // GC what should we do here -- return or carry on and check destination exists from a previous run?
       }
    }

    // If 'source' is empty, the 'jf_' link wasn't there so we assume the real zip file is already in the working directory.
    // We could assume that the real zip file has already been unzipped, but to be safe unzip it if found.
    if (file_exists(destination) ) {
       std::cerr << "Unzipping the " << type << " zip file: " << destination << '\n';
       if (!cpdn_unzip(destination, unzip_path)) {
         std::cerr << "..Unzipping the " << type << " file failed" << std::endl;
         return 1;
       }
    }
    else {
       std::cerr << "..The " << type << " file does not exist in the working directory: " << destination << std::endl;
       return 1;
    }

	 // Success, retval is 0
    return retval;
}


