# MangoH-Waveshare
For integration MangoH Red and Wave Share E-Ink Display


### 1. Download Legato Distribution Source Package from sierra wireless website:
    Yocto Source (Around 4 GB)
    The file name is Legato-Dist-Source-mdm9x15-SWI9X15Y_07.10.04.00.tar.bz2

### 2. Extract the downloaded Distribution source package:

    Run command: tar -xvjf Legato-Dist-Source-mdm9x15-SWI9X15Y_07.10.04.00.tar.bz2

### 3. Disable Legato configuration to build Yocto

    Run command: cd yocto
    Run command: export LEGATO_BUILD=0

### 4. Build Yocto images from source

    Run command: make image_bin
    If you get the error not found 'serf.h', then follow the steps below, otherwise skip to the next step.
        Go to directory: cd meta-swi/common/recipes-devtools/
        create directory: mkdir subversion
        Put attach file: subversion_1.8.9.bbappend, under directory subversion
        The full path would be: yocto/meta-swi/common/recipes-devtools/subversion/subversion_1.8.9.bbappend
        build Yocto images again: make image_bin
### 5. Set environment under yocto directory, this will get command bitbake run:

    Run Command: . ./poky/oe-init-build-env
    
### 6. Build Linux kernel with Buffer Frame driver modules support

    Go to directory: yocto/build_bin (cd …/build_bin)
    Configure kernel with default:
        Run command: bitbake linux-yocto -c kernel_configme -f
    Configure kernel to add BT driver module in Linux configuration
        Run command: bitbake linux-yocto -c menuconfig
        Enter Device Driver -> Graphic Support
        Enter <M> Suport for frameBuffer devices
            <M> Ion Memory Manager
            <> Lowlevel video output switch controls
            <*> Support for frame buffer devices  
            Console display driver support 
            <*> Framebuffer Console support
            [*]     Map the console to primary display device
            [*]     Framebuffer Console Rotation
    Exit and save Frame Buffer Linux config
    Rebuild kernel image: bitbake -f linux-yocto
 
 ### 7. Rebuild rootfs filesystem and images

    bitbake -c cleansstate mdm9x15-image-minimal
    bitbake mdm9x15-image-minimal

Check new build CWE images: yocto_wp85.cwe

    Go to images directory: cd build_bin/tmp/deploy/images/swi-mdm9x15

Reflash new image with Frame Buffer driver module support to the target board

    on Windows: FDT command: fdt yocto_wp85.cwe 
    on Linux: by command: fwupdate download yocto_wp85.cwe




