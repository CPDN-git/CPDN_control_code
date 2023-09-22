//
// Control code functions for the climateprediction.net project (CPDN)
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include "CPDN_control_code.h"

// Initialise BOINC and set the options
int initialise_boinc(std::string wu_name, std::string project_dir, std::string version) {
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
    return boinc_init_options(&options);
}


// Move and unzip the app file
int move_and_unzip_app_file(std::string app_name, std::string version, std::string project_path, std::string slot_path) {
    int retval = 0;

    // macOS
    #if defined (__APPLE__)
       std::string app_file = app_name + std::string("_app_") + version + std::string("_x86_64-apple-darwin.zip");
    // ARM
    #elif defined (_ARM) 
       std::string app_file = app_name + std::string("_app_") + version + std::string("_aarch64-poky-linux.zip");
    // Linux
    #else
       std::string app_file = app_name + std::string("_app_") + version + std::string("_x86_64-pc-linux-gnu.zip");
    #endif

    // Copy the app file to the working directory
    std::string app_source = project_path + app_file;
    std::string app_destination = slot_path + std::string("/") + app_file;
    cerr << "Copying: " << app_source << " to: " << app_destination << "\n";
    retval = boinc_copy(app_source.c_str(), app_destination.c_str());
    if (retval) {
       cerr << "..Copying the app file to the working directory failed: error " << retval << "\n";
       return retval;
    }

    // Unzip the app zip file
    std::string app_zip = slot_path + std::string("/") + app_file;
    cerr << "Unzipping the app zip file: " << app_zip << "\n";
    retval = boinc_zip(UNZIP_IT, app_zip.c_str(), slot_path);

    if (retval) {
       cerr << "..Unzipping the app file failed" << "\n";
       return retval;
    }
    // Remove the zip file
    else {
       std::remove(app_zip.c_str());       
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


long launch_process_oifs(const std::string slot_path, const char* strCmd, const char* exptid, const std::string app_name) {
    int retval = 0;
    long handleProcess;

    //cerr << "slot_path: " << slot_path << "\n";
    //cerr << "strCmd: " << strCmd << "\n";
    //cerr << "exptid: " << exptid << "\n";

    switch((handleProcess=fork())) {
       case -1: {
          cerr << "..Unable to start a new child process" << "\n";
          exit(0);
          break;
       }
       case 0: { //The child process
          char *pathvar=NULL;
          // Set the GRIB_SAMPLES_PATH environmental variable
          std::string GRIB_SAMPLES_var = std::string("GRIB_SAMPLES_PATH=") + slot_path + \
                                         std::string("/eccodes/ifs_samples/grib1_mlgrib2");
          if (putenv((char *)GRIB_SAMPLES_var.c_str())) {
            cerr << "..Setting the GRIB_SAMPLES_PATH failed" << "\n";
          }
          pathvar = getenv("GRIB_SAMPLES_PATH");
          cerr << "The GRIB_SAMPLES_PATH environmental variable is: " << pathvar << "\n";

          // Set the GRIB_DEFINITION_PATH environmental variable
          std::string GRIB_DEF_var = std::string("GRIB_DEFINITION_PATH=") + slot_path + \
                                     std::string("/eccodes/definitions");
          if (putenv((char *)GRIB_DEF_var.c_str())) {
            cerr << "..Setting the GRIB_DEFINITION_PATH failed" << "\n";
          }
          pathvar = getenv("GRIB_DEFINITION_PATH");
          cerr << "The GRIB_DEFINITION_PATH environmental variable is: " << pathvar << "\n";

          if((app_name=="openifs") || (app_name=="oifs_40r1")) { // OpenIFS 40r1
            cerr << "Executing the command: " << strCmd << " -e " << exptid << "\n";
            retval = execl(strCmd,strCmd,"-e",exptid,NULL);
          }
          else {  // OpenIFS 43r3 and above
            cerr << "Executing the command: " << strCmd << "\n";
            retval = execl(strCmd,strCmd,NULL,NULL,NULL);
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


// Update the progress file
void update_progress_file(std::string progress_file, int last_cpu_time, int upload_file_number,
                          std::string last_iter, int last_upload, int model_completed) {

    std::ofstream progress_file_out(progress_file);
    cerr << "Writing to progress file: " << progress_file << "\n";

    // Write out the new progress file. Note this truncates progress_file to zero bytes if it already exists (as in a model restart)
    progress_file_out.open(progress_file);
    progress_file_out <<"<?xml version=\"1.0\" encoding=\"utf-8\"?>"<< '\n';
    progress_file_out <<"<running_values>"<< '\n';
    progress_file_out <<"  <last_cpu_time>"<<std::to_string(last_cpu_time)<<"</last_cpu_time>"<< '\n';
    progress_file_out <<"  <upload_file_number>"<<std::to_string(upload_file_number)<<"</upload_file_number>"<< '\n';
    progress_file_out <<"  <last_iter>"<<last_iter<<"</last_iter>"<< '\n';
    progress_file_out <<"  <last_upload>"<<std::to_string(last_upload)<<"</last_upload>"<< '\n';
    progress_file_out <<"  <model_completed>"<<std::to_string(model_completed)<<"</model_completed>"<< '\n';
    progress_file_out <<"</running_values>"<< std::endl;
    progress_file_out.close();

    cerr << "last_cpu_time: " << last_cpu_time << "\n";
    cerr << "upload_file_number: " << upload_file_number << "\n";
    cerr << "last_iter: " << last_iter << "\n";
    cerr << "last_upload: " << last_upload << "\n";
    cerr << "model_completed: " << model_completed << "\n";
}


// Produce the trickle and either upload to the project server or as a physical file
void process_trickle(double current_cpu_time, std::string wu_name, std::string result_base_name, std::string slot_path, int timestep) {
    std::string trickle, trickle_location;
    int rsize;

    //cerr << "current_cpu_time: " << current_cpu_time << "\n";
    //cerr << "wu_name: " << wu_name << "\n";
    //cerr << "result_base_name: " << result_base_name << "\n";
    //cerr << "slot_path: " << slot_path << "\n";
    //cerr << "timestep: " << timestep << "\n";

    std::stringstream trickle_buffer;
    trickle_buffer << "<wu>" << wu_name << "</wu>\n<result>" << result_base_name << "</result>\n<ph></ph>\n<ts>" \
                   << timestep << "</ts>\n<cp>" << current_cpu_time << "</cp>\n<vr></vr>\n";
    trickle = trickle_buffer.str();
    cerr << "Contents of trickle: " << trickle << "\n";
      
    // Upload the trickle if not in standalone mode
    if (!boinc_is_standalone()) {
       std::string variety("orig");
       cerr << "Uploading trickle at timestep: " << timestep << "\n";
       boinc_send_trickle_up(variety.data(), const_cast<char*> (trickle.c_str()));
    }

    // Write out the trickle in standalone mode
    else {
       std::stringstream trickle_location_buffer;
       trickle_location_buffer << slot_path << "/trickle_" << time(NULL) << ".xml" << "\n";
       trickle_location = trickle_location_buffer.str();
       cerr << "Writing trickle to location: " << trickle_location << "\n";
       FILE* trickle_file = fopen(trickle_location.c_str(), "w");
       if (trickle_file) {
          //fwrite(trickle_file, 1, strlen(trickle.c_str()), trickle.c_str());
          //fwrite(trickle.c_str(), 1, strlen(trickle.c_str()), trickle_file);
          std::fwrite(trickle.c_str(), sizeof(char), sizeof(trickle.c_str()), trickle_file);
          fclose(trickle_file);
       }
    }
}

// Check whether a file exists
bool file_exists(const std::string& filename)
{
    std::ifstream infile(filename.c_str());
    return infile.good();
}

// Check whether file is zero bytes long
// from: https://stackoverflow.com/questions/2390912/checking-for-an-empty-file-in-c
// returns True if file is zero bytes, otherwise False.
bool file_is_empty(std::string& fpath) {
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
       //tv_sec = usage.ru_utime.tv_sec; //Time spent executing in user mode (seconds)
       //tv_usec = usage.ru_utime.tv_usec; //Time spent executing in user mode (microseconds)
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
   // Impact of speedup due to multiple threads is accounted by below.
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

// Construct the second part of the file to be uploaded
std::string get_second_part(string last_iter, string exptid) {
   std::string second_part="";

   if (last_iter.length() == 1) {
      second_part = exptid +"+"+ "00000" + last_iter;
   }
   else if (last_iter.length() == 2) {
      second_part = exptid + "+" + "0000" + last_iter;
   }
   else if (last_iter.length() == 3) {
      second_part = exptid + "+" + "000" + last_iter;
   }
   else if (last_iter.length() == 4) {
      second_part = exptid + "+" + "00" + last_iter;
   }
   else if (last_iter.length() == 5) {
      second_part = exptid + "+" + "0" + last_iter;
   }
   else if (last_iter.length() == 6) {
      second_part = exptid + "+" + last_iter;
   }

   return second_part;
}


int move_result_file(std::string slot_path, std::string temp_path, std::string first_part, std::string second_part) {
    int retval = 0;

    // Move result file to the temporary folder in the project directory
    if(file_exists(slot_path + std::string("/") + first_part + second_part)) {
       cerr << "Moving to projects directory: " << (slot_path+std::string("/") + first_part + second_part) << "\n";
       retval = boinc_copy((slot_path + std::string("/") + first_part + second_part).c_str() , \
                           (temp_path + std::string("/") + first_part + second_part).c_str());

       // If result file has been successfully copied over, remove it from slots directory
       if (!retval) {
          std::remove((slot_path + std::string("/") + first_part + second_part).c_str());
       }
    }
    return retval;
}


bool check_stoi(std::string& cin) {
    //  check input string is convertable to an integer by checking for any letters
    //  nb. stoi() will convert leading digits if alphanumeric but we know step must be all digits.
    //  Returns true on success, false if non-numeric data in input string.
    //  Glenn Carver

    int step;

    if (std::any_of(cin.begin(), cin.end(), ::isalpha)) {
        cerr << "..Invalid characters in stoi string: " << cin << "\n";
        return false;
    }

    //  check stoi standard exceptions
    //  n.b. still need to check step <= max_step
    try {
        step = std::stoi(cin);
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

bool oifs_parse_stat(std::string& logline, std::string& stat_column, int index) {
   //   Parse a line of the OpenIFS ifs.stat log file, previously obtained from oifs_get_statline
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
      return false;
   } else {
      stat_column = statstr;
      //cerr << "oifs_parse_ifsstat: parsed string  = " << stat_column << " index " << index << '\n';
      return true;
   }
}

bool oifs_get_stat(std::ifstream& ifs_stat, std::string& logline) {
   // Parse content of ifs.stat and always return last non-zero line read from log file.
   //
   // Updates stream offset between calls to prevent completely re-reading the file,
   // to reduce file I/O on the volunteer's machine.
   //
   //    ifs_stat : name of logfile (ifs.stat for current generation of OpenIFS models)
   //    logline  : last line read from ifs.stat. Preserved between calls to this fn.
   //    NOTE!  The file MUST already be open. This fn does not close it.
   //
   // Returns: False if file not open, otherwise true.
   //
   // TODO: ideally this should be part of a small class that
   // inherits from ifstream to manage & read ifs.stat, as it relies on trust the
   // callee has not opened & closed this file inbetween calls.
   //
   //     Glenn

    string             statline = "";         // default: 4th element of ifs.stat file lines
    static string      current_line = "";
    static streamoff   p = 0;             // stream offset position

    if ( !ifs_stat.is_open() ) {
        cerr << "..oifs_get_stat: error, ifs.stat file is not open" << endl;
        p = 0;
        current_line = "";
        return false;
    }

    ifs_stat.seekg(p);
    while ( std::getline(ifs_stat, statline) ) {
      current_line = statline;

      if ( ifs_stat.tellg() == -1 )     // set p to eof for next call to this fn
         p = p + statline.size();
      else
         p = ifs_stat.tellg();
    }
    ifs_stat.clear();           // must clear stream error before attempting to read again as file remains open

    logline = current_line;

    return true;
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
    
    int count;
    std::string delimiter = "\"";
    size_t pos = 0;
    std::string rcf_file_line;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line )) {

       if (rcf_file_line.find("CSTEP") != std::string::npos ) {
          count = 0;
          // From the CSTEP string take the second field delimited by speechmarks
          while ((pos = rcf_file_line.find(delimiter)) != std::string::npos) {
             count = count + 1;
             if (count == 2) { 
                cstep_value = rcf_file_line.substr(0,pos);
                // Remove whitespace
                cstep_value.erase( std::remove_if( cstep_value.begin(), \
                                   cstep_value.end(), ::isspace ), cstep_value.end() );
             }
             rcf_file_line.erase(0, pos + delimiter.length());
          }
       }

       if (rcf_file_line.find("CTIME") != std::string::npos ) {
          count = 0;
          // From the CTIME string take the second field delimited by speechmarks
          while ((pos = rcf_file_line.find(delimiter)) != std::string::npos) {
             count = count + 1;
             if (count == 2) { 
                ctime_value = rcf_file_line.substr(0,pos);
                // Remove whitespace
                ctime_value.erase( std::remove_if( ctime_value.begin(), \
                                   ctime_value.end(), ::isspace ), ctime_value.end() );
             }
             rcf_file_line.erase(0, pos + delimiter.length());
          }
       }

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
