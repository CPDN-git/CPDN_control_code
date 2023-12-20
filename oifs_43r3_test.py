#! /usr/bin/python3

# Script to simulate an oifs_43r3 executable for testing purposes

# This script has been written by Andy Bowery (Oxford University, 2023)

if __name__ == "__main__":

    import argparse, time, secrets
    from datetime import datetime

    # This script is run from the command line using: ./oifs_43r3_test.py --nfrres=N --upload_interval=N


    # Parse the command line, use argparse to read in the options from the shell command line
    parser = argparse.ArgumentParser()
    parser.add_argument("--nfrres",default=10000)
    parser.add_argument("--upload_interval",default=10000)
    options = parser.parse_args()


    print("Starting oifs_43r3_test: "+str(datetime.now()))

    time.sleep(2)

    iteration = 0
    max_iter = 5

    #options.upload_interval = 2


    while iteration <= max_iter:

       if iteration % abs( int(options.nfrres) ) == 0:
          iteration2 = 3
       else:
          iteration2 = 1

       # Write to the ifs.stat file
       for x in range(iteration2):
          with open('ifs.stat', 'a') as ifs_stat_file:
             if len(str(iteration)) == 1:
                ifs_stat_file.write(' ' + datetime.now().strftime("%H:%M:%S") + ' 0AAA00AAA STEPO       ' + str(iteration) + '\n')
             if len(str(iteration)) == 2:
                ifs_stat_file.write(' ' + datetime.now().strftime("%H:%M:%S") + ' 0AAA00AAA STEPO      ' + str(iteration) + '\n')


       # Write out the ICM files if at the end of an upload_interval
       if iteration % int(options.upload_interval) == 0 and iteration > 0:
          if len(str(iteration)) == 1:
             second_part = '00000' + str(iteration)
          elif len(str(iteration)) == 2:
             second_part = '0000' + str(iteration)
          elif len(str(iteration)) == 3:
             second_part = '000' + str(iteration)

          # Write to the ICMGG
          with open('ICMGG' + second_part, 'a') as ICMGG_file:
             ICMGG_file.write(secrets.token_hex(4000) + '\n')

          # Write to the ICMSH file
          with open('ICMSH' + second_part, 'a') as ICMSH_file:
             ICMSH_file.write(secrets.token_hex(4000) + '\n')

          # Write to the ICMUA file
          with open('ICMUA' + second_part, 'a') as ICMUA_file:
             ICMUA_file.write(secrets.token_hex(4000) + '\n')

       iteration = iteration + 1



    # And finally write the last CNTO line into the ifs.stat file
    with open('ifs.stat', 'a') as ifs_stat_file:
       if len(str(iteration)) == 1:
          ifs_stat_file.write(' ' + datetime.now().strftime("%H:%M:%S") + ' 0AAA00AAA CNTO        ' + str(iteration-1) + '\n')
       if len(str(iteration)) == 2:
          ifs_stat_file.write(' ' + datetime.now().strftime("%H:%M:%S") + ' 0AAA00AAA CNTO       ' + str(iteration-1) + '\n')


    # Produce NODE file
    with open('NODE.001_01', 'a') as node_file:
       node_file.write(secrets.token_hex(4000) + '\n')
