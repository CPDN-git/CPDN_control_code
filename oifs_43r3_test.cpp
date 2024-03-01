// oifs_43r3_test.exe

// Program to simulate an oifs_43r3 executable for testing purposes
// This program has been written by Andy Bowery (Oxford University, 2023)

// To build use: g++ oifs_43r3_test.cpp -std=c++17 -o oifs_43r3_test.exe

// This program is run from the command line using: ./oifs_43r3_test.exe N N

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>

using namespace std;
using namespace std::chrono;


int main(int argc, char** argv) {

    std::string second_part;
    int i, j, iteration = 0, max_iter = 5, iteration2 = 0;

    // Read nfrres and upload_interval
    std::string nfrres = argv[1]; // restart interval
    std::string upload_interval = argv[2];

    cerr << "Starting oifs_43r3_test" << std::endl;

    // Get the slots path (the current working path)
    std::string slot_path = std::filesystem::current_path();

    std::string ifs_stat_file = slot_path + std::string("/ifs.stat");
    std::ofstream ifs_stat_file_out(ifs_stat_file);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);


    while (iteration <= max_iter) {

       // Time delay
       //this_thread::sleep_until(system_clock::now() + seconds(60));


       // At the end of every restart interval, write out the same line three times
       if ( iteration % abs( std::stoi(nfrres) ) == 0) {
          iteration2 = 3;
       } else {
          iteration2 = 1;
       }

       // Write to the ifs.stat file
       for (i=0; i < iteration2; i++) {
          if ( to_string(iteration).length() == 1) {
             ifs_stat_file_out <<" "<< std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA STEPO       " << to_string(iteration) << std::endl;
          } else if ( to_string(iteration).length() == 2) {
             ifs_stat_file_out <<" "<< std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA STEPO      " << to_string(iteration) << std::endl;
          }
       }

       // Write out the ICM files if at the end of an upload_interval
       if (iteration % stoi(upload_interval) == 0 and iteration > 0) {
          if ( to_string(iteration).length() == 1) {
             second_part = "00000" + to_string(iteration);
          } else if ( to_string(iteration).length() == 2) {
             second_part = "0000" + to_string(iteration);
          } else if ( to_string(iteration).length() == 3) {
             second_part = "000" + to_string(iteration);
          }

          std::string ICMGG_file = slot_path + std::string("/ICMGG") + second_part;
          std::string ICMSH_file = slot_path + std::string("/ICMSH") + second_part;
          std::string ICMUA_file = slot_path + std::string("/ICMUA") + second_part;

          // Open the ICM file streams
          std::ofstream ICMGG_file_out(ICMGG_file);
          std::ofstream ICMSH_file_out(ICMSH_file);
          std::ofstream ICMUA_file_out(ICMUA_file);

          // Write to the ICMGG
          for (j=0;j<4000;j++) { ICMGG_file_out << rand() % 10; };
          ICMGG_file_out << std::endl;

          // Write to the ICMSH
          for (j=0;j<4000;j++) { ICMSH_file_out << rand() % 10; };
          ICMSH_file_out << std::endl;

          // Write to the ICMUA
          for (j=0;j<4000;j++) { ICMUA_file_out << rand() % 10; };
          ICMUA_file_out << std::endl;

          // Close the ICM file streams
          ICMGG_file_out.close();
          ICMSH_file_out.close();
          ICMUA_file_out.close();

       }

       iteration = iteration + 1;
    }


    // And finally write the last CNTO line into the ifs.stat file
    if ( to_string(iteration).length() == 1) {
       ifs_stat_file_out <<" "<< std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA CNTO        " << to_string(iteration) << std::endl;
    } else if ( to_string(iteration).length() == 2) {
       ifs_stat_file_out <<" "<< std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA CNTO       " << to_string(iteration) << std::endl;
    }
    ifs_stat_file_out.close();


    // Produce the NODE file
    std::string NODE_file = slot_path + std::string("/NODE.001_01");
    std::ofstream NODE_file_out(NODE_file);
    for (j=0;j<4000;j++) { NODE_file_out << rand() % 10; };
    NODE_file_out << std::endl;
    NODE_file_out.close();

}
