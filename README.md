# XDAQ-RHX: Fork of Intan-RHX for Exclusive XDAQ Hardware Compatibility

## Important Notice
* **Compatibility**: This modified release is **not** compatible with Intan Recording or StimRecord Controllers. To use Intan's controller hardware, please refer to the original RHX software provided by Intan Technologies.
* **Stim-Record / Record Modes**: The XDAQ configuration supports either Stim-Record (X3SR) or Record (X3R/X6R) X-Headstages exclusively. Concurrent use of SR and R X-Headstages is not supported at this time. Future updates may include this capability, depending on user demand.
* **Port Usage**: Always use Port 1 to drive the X-Headstages or other Intan Compatible headstages.  Other ports might not be activated depending on purchased configurations.
- **IO Expander Limitation**: The current version of RHX XDAQ does not support XDAQ IO Expander bits 17-32. This limitation is under review for future updates.
- **Port Layout Differences**: The port layout for XDAQ differs from that of standard Intan Controllers, specifically when configured for use with Record X-Headstages. Users should refer to the port mapping below for accurate setup.
  | XDAQ HDMI Port | RHX Application|
  | - | - |
  | Port 1 ( 0~127 ch) | Port A |
  | Port 1 (128~255 ch) | Port B |
  | Port 2 ( 0~127 ch) | Port C |
  | Port 2 (128~255 ch) | Port D |
  | Port 3 ( 0~127 ch) | Port E |
  | Port 3 (128~255 ch) | Port F |
  | Port 4 ( 0~127 ch) | Port G |
  | Port 4 (128~255 ch) | Port H |

# Intan-RHX
Intan RHX is free, powerful data acquisition software that displays and records electrophysiological signals from any Intan RHD or RHS system using an RHD USB interface board, RHD recording controller, or RHS stim/recording controller.

The most recent binaries are available from the Intan website: https://intantech.com or here on GitHub under Releases:

* IntanRHXInstaller.exe -> Windows 64-bit installer (Wix Burn bootstrapper application that guides the user through installation)

* IntanRHX.dmg -> MacOS 64-bit disk image

* IntanRHX.tar.gz -> Linux 64-bit archive file

These binaries were built with Qt 5.12.8.

While developers are free to download the source code or fork their own repositories if they wish to make changes to their own versions, we will generally not integrate any of these changes to a public release. If there are features you'd like to see in an official Intan release or you find a bug, please send us feedback!  Thank  you!

# Steps To Run Software
## Project Setup Guide
This project is designed with CMake as its underlying build system, providing a versatile setup for development across different platforms. To streamline the development process, we suggest using an Integrated Development Environment (IDE) that offers robust support for CMake projects. Our recommendations include Qt Creator for those who prefer a Qt-focused development environment and Visual Studio for developers leaning towards a comprehensive, Windows-based IDE.

### Getting Started
#### For Qt Creator Users
Begin by opening the CMakeLists.txt file in Qt Creator. Please note that you should open the CMakeLists.txt file directly, not the .pro file, to ensure proper project configuration and build settings.

#### For Visual Studio Users
Before opening the project in Visual Studio, you'll need to generate the solution files using CMake. Open a terminal or command prompt in the project's root directory and run the following command:
``cmake -S . -B Build``

This command configures the project and creates a Build directory containing the Visual Studio solution file (.sln). Next, navigate to the Build folder and open the generated .sln file in Visual Studio to start working on the project.

#### Environment Configuration

Regardless of the platform or IDE you choose, it's essential to configure the Qt5 binaries as an environment variable. This step is critical for ensuring that the program runs correctly. The environment variable should point to the location where the Qt5 binaries are installed on your system.

Please refer to the official Qt documentation for detailed instructions on setting up environment variables for Qt5 on your specific operating system.

## Prerequisites

Before running or contributing to the project, ensure that you have installed the necessary dependencies specific to your operating system. The dependencies are crucial for the successful compilation and execution of the project.

Follow the steps below based on your development platform:

### Windows

The RHX software depends on Opal Kelly USB drivers and Microsoft Redistributables. When running the distributed Windows installer from the Intan website, these are automatically installed, but when building RHX from source, these should still be installed on the system. Opal Kelly USB drivers should be installed so that the Intan hardware can communicate via USB. These are available at: https://intantech.com/files/Intan_controller_USB_drivers.zip. These also rely on the Microsoft Visual C++ Redistributables (x64) from 2010, 2013, and 2015-2019, which are available from Microsoft and should also be installed prior to running IntanRHX. Finally, okFrontPanel.dll (found in the libraries directory) should be in the same directory as the binary executable at runtime.

### Mac

libokFrontPanel.dylib should be in a directory called "Frameworks" alongside the MacOS directory within the built IntanRHX.app. Running macdeployqt on this application will also populate this directory with required Qt libraries.

### Linux

A udev rules file should be added so that the Intan hardware can communicate via USB. The 60-opalkelly.rules file should be copied to /etc/udev/rules.d/, after which the system should be restarted or the command 'udevadm control --reload-rules' should be run. libokFrontPanel.so should be in the same directory as the binary executable at runtime. 