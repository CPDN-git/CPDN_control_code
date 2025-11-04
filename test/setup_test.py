#! /usr/bin/python3

# Script to setup the file structure required to run a test of new checked in code using GitHub Action 
# or run locally.  This test mimics a workunit for the OpenIFS 43r3 model.

#   Glenn Carver, CPDN Development Team, Nov 2025.
#   Based on earlier version by Andy Bowery.

if __name__ == "__main__":

    import os, secrets, zipfile, shutil

    # Make sure we're in the 'test' directory, if not then exit with error
    current_path = os.getcwd()
    if not current_path.endswith("test"):
      print("Error: test_setup.py must be run from within the 'test' directory")
      exit(1)

    if not os.path.isdir("projects"):
      os.mkdir("projects")

    if not os.path.isdir("slot"):
      os.mkdir("slot")

    os.chdir("projects")

    # Produce fake oifs_43r3_app file
    with open('oifs_43r3_app_1.00_x86_64-pc-linux-gnu', 'a') as oifs_app_file:
      oifs_app_file.write(secrets.token_hex(4000) + '\n')
    zip_file = zipfile.ZipFile('./oifs_43r3_app_1.00_x86_64-pc-linux-gnu.zip','w')
    zip_file.write('oifs_43r3_app_1.00_x86_64-pc-linux-gnu')
    zip_file.close()

    os.chdir("../slot")

    # Create the init_data.xml, needed by BOINC
    init_data_string = "   <app_init_data>\n" +\
                       "     <major_version>0</major_version>\n" +\
                       "     <minor_version>0</minor_version>\n" +\
                       "     <release>0</release>\n" +\
                       "     <app_version>000</app_version>\n" +\
                       "     <hostid>0</hostid>\n" +\
                       "     <app_name>openifs</app_name>\n" +\
                       "     <project_preferences></project_preferences>\n" +\
                       "     <project_dir>"+current_path+"/projects</project_dir>\n" +\
                       "     <boinc_dir>"+current_path+"</boinc_dir>\n" +\
                       "     <wu_name>oifs_43r3_NNNN_yyyymmddhh_1_d000_0</wu_name>\n" +\
                       "     <shm_key>0</shm_key>\n" +\
                       "     <slot>0</slot>\n" +\
                       "     <wu_cpu_time>0.000000</wu_cpu_time>\n" +\
                       "     <starting_elapsed_time>0.000000</starting_elapsed_time>\n" +\
                       "     <user_total_credit>0.000000</user_total_credit>\n" +\
                       "     <user_expavg_credit>0.000000</user_expavg_credit>\n" +\
                       "     <host_total_credit>0.000000</host_total_credit>\n" +\
                       "     <host_expavg_credit>0.000000</host_expavg_credit>\n" +\
                       "     <resource_share_fraction>0.000000</resource_share_fraction>\n" +\
                       "     <checkpoint_period>60.000000</checkpoint_period>\n" +\
                       "     <fraction_done_start>0.000000</fraction_done_start>\n" +\
                       "     <fraction_done_end>1.000000</fraction_done_end>\n" +\
                       "     <rsc_fpops_est>0.000000</rsc_fpops_est>\n" +\
                       "     <rsc_fpops_bound>0.000000</rsc_fpops_bound>\n" +\
                       "     <rsc_memory_bound>0.000000</rsc_memory_bound>\n" +\
                       "     <rsc_disk_bound>0.000000</rsc_disk_bound>\n" +\
                       "     <computation_deadline>0.000000</computation_deadline>\n" +\
                       "     <host_info></host_info>\n" +\
                       "     <proxy_info></proxy_info>\n" +\
                       "     </global_preferences>\n" +\
                       "   </app_init_data>\n"

    init_data_file=open("init_data.xml","w")
    init_data_file.write(init_data_string)
    init_data_file.close()


    # Create the fake model namelist file, fort.4.
    fort_file_string = "!WU_TEMPLATE_VERSION=43r3-seasonal-20250801\n"+\
                         "!EXPTID=NNNN\n"+\
                         "!UNIQUE_MEMBER_ID=1353\n"+\
                         "!IFSDATA_FILE=ifsdata_0\n" +\
                         "!IC_ANCIL_FILE=ic_ancil_0\n" +\
                         "!CLIMATE_DATA_FILE=clim_data_0\n" +\
                         "!HORIZ_RESOLUTION=159\n" +\
                         "!VERT_RESOLUTION=91\n" +\
                         "!GRID_TYPE=l_2\n" +\
                         "!UPLOAD_INTERVAL=288.0\n" +\
                         "!TSTEP=3600\n" +\
                         "&NAMCT0\n"+\
                         " UTSTEP=3600.0,\n" +\
                         " NFRPOS=1,\n" +\
                         " NFRRES=1,\n"

    fort_file=open("fort.4","w")
    fort_file.write(fort_file_string)
    fort_file.close()
    zip_file = zipfile.ZipFile('./jf_namelist','w')
    zip_file.write('fort.4')
    zip_file.close()

    # The OpenIFS BOINC implementation uses mapped logical filenames to
    # identify the various input files.  Here we create the files with
    # the logical names expected by the model.
    # It is less than ideal as it makes the code more complex to handle
    # resolving these files. It also makes it harder to debug as the
    # filenames are not descriptive.

    # Create logical namelist file
    namelist_string = ">jf_namelist<\n"
    namelist_file=open("oifs_43r3_NNNN_yyyymmddhh_1_d000_0.zip","w")
    namelist_file.write(namelist_string)
    namelist_file.close()

    # Create ic_ancil file
    ic_ancil_string = ">jf_ic_ancil<\n"
    ic_ancil_file=open("ic_ancil_0.zip","w")
    ic_ancil_file.write(ic_ancil_string)
    ic_ancil_file.close()

    # Produce jf_ic_ancil file
    with open('jf_ic_ancil', 'a') as jf_ic_ancil_file:
      jf_ic_ancil_file.write(secrets.token_hex(4000) + '\n')
    zip_file = zipfile.ZipFile('./jf_ic_ancil.zip','w')
    zip_file.write('jf_ic_ancil')
    zip_file.close()
    shutil.move('./jf_ic_ancil.zip','./jf_ic_ancil')

    # Create ifsdata file
    ifsdata_string = ">jf_ifsdata<\n"
    ifsdata_file=open("ifsdata_0.zip","w")
    ifsdata_file.write(ifsdata_string)
    ifsdata_file.close()

    # Produce jf_ifsdata file
    with open('jf_ifsdata', 'a') as jf_ifsdata_file:
      jf_ifsdata_file.write(secrets.token_hex(4000) + '\n')
    zip_file = zipfile.ZipFile('./jf_ifsdata.zip','w')
    zip_file.write('jf_ifsdata')
    zip_file.close()
    shutil.move('./jf_ifsdata.zip','./jf_ifsdata')

    # Create clim_data file
    clim_data_string = ">jf_clim_data<\n"
    clim_data_file=open("clim_data_0.zip","w")
    clim_data_file.write(clim_data_string)
    clim_data_file.close()

    # Produce jf_clim_data file
    with open('jf_clim_data', 'a') as jf_clim_data_file:
      jf_clim_data_file.write(secrets.token_hex(4000) + '\n')
    zip_file = zipfile.ZipFile('./jf_clim_data.zip','w')
    zip_file.write('jf_clim_data')
    zip_file.close()
    shutil.move('./jf_clim_data.zip','./jf_clim_data')
