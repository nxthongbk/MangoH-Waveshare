# MangoH-Waveshare
For integration MangoH Red and Wave Share E-Ink Display
Download Legato Distribution Source Package from sierra wireless website:

    Yocto Source (Around 3 GB)
    The file name is Legato-Dist-Source-mdm9x15-SWI9X15Y_07.10.04.00.tar.bz2

Extract the downloaded Distribution source package:

    Run command: tar -xvjf Legato-Dist-Source-mdm9x15-SWI9X15Y_07.10.04.00.tar.bz2

Disable Legato configuration to build Yocto

    Run command: cd yocto
    Run command: export LEGATO_BUILD=0

Build Yocto images from source

    Run command: make image_bin
    If you get the error not found 'serf.h', then follow the steps below, otherwise skip to the next step.
        Go to directory: cd meta-swi/common/recipes-devtools/
        create directory: mkdir subversion
        Put attach file: subversion_1.8.9.bbappend, under directory subversion
        The full path would be: yocto/meta-swi/common/recipes-devtools/subversion/subversion_1.8.9.bbappend
        build Yocto images again: make image_bin

