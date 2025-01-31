set(EngineSources
    Engine/API/Abstract/abstractrhxcontroller.cpp
    Engine/API/Abstract/abstractrhxcontroller.h
    Engine/API/Hardware/controller_info.cpp
    Engine/API/Hardware/controller_info.h
    Engine/API/Hardware/rhxcontroller.cpp
    Engine/API/Hardware/rhxcontroller.h
    Engine/API/Hardware/rhxdatablock.cpp
    Engine/API/Hardware/rhxdatablock.h
    Engine/API/Hardware/rhxglobals.h
    Engine/API/Hardware/rhxregisters.cpp
    Engine/API/Hardware/rhxregisters.h
    Engine/API/Synthetic/playbackrhxcontroller.cpp
    Engine/API/Synthetic/playbackrhxcontroller.h
    Engine/API/Synthetic/randomnumber.cpp
    Engine/API/Synthetic/randomnumber.h
    Engine/API/Synthetic/synthdatablockgenerator.cpp
    Engine/API/Synthetic/synthdatablockgenerator.h
    Engine/API/Synthetic/syntheticrhxcontroller.cpp
    Engine/API/Synthetic/syntheticrhxcontroller.h
    Engine/Processing/DataFileReaders/datafile.cpp
    Engine/Processing/DataFileReaders/datafile.h
    Engine/Processing/DataFileReaders/datafilemanager.cpp
    Engine/Processing/DataFileReaders/datafilemanager.h
    Engine/Processing/DataFileReaders/datafilereader.cpp
    Engine/Processing/DataFileReaders/datafilereader.h
    Engine/Processing/DataFileReaders/fileperchannelmanager.cpp
    Engine/Processing/DataFileReaders/fileperchannelmanager.h
    Engine/Processing/DataFileReaders/filepersignaltypemanager.cpp
    Engine/Processing/DataFileReaders/filepersignaltypemanager.h
    Engine/Processing/DataFileReaders/traditionalintanfilemanager.cpp
    Engine/Processing/DataFileReaders/traditionalintanfilemanager.h
    Engine/Processing/SaveManagers/fileperchannelsavemanager.cpp
    Engine/Processing/SaveManagers/fileperchannelsavemanager.h
    Engine/Processing/SaveManagers/filepersignaltypesavemanager.cpp
    Engine/Processing/SaveManagers/filepersignaltypesavemanager.h
    Engine/Processing/SaveManagers/intanfilesavemanager.cpp
    Engine/Processing/SaveManagers/intanfilesavemanager.h
    Engine/Processing/SaveManagers/savefile.cpp
    Engine/Processing/SaveManagers/savefile.h
    Engine/Processing/SaveManagers/savemanager.cpp
    Engine/Processing/SaveManagers/savemanager.h
    Engine/Processing/XPUInterfaces/abstractxpuinterface.cpp
    Engine/Processing/XPUInterfaces/abstractxpuinterface.h
    Engine/Processing/XPUInterfaces/cpuinterface.cpp
    Engine/Processing/XPUInterfaces/cpuinterface.h
    Engine/Processing/XPUInterfaces/gpuinterface.cpp
    Engine/Processing/XPUInterfaces/gpuinterface.h
    Engine/Processing/XPUInterfaces/xpucontroller.cpp
    Engine/Processing/XPUInterfaces/xpucontroller.h
    Engine/Processing/channel.cpp
    Engine/Processing/channel.h
    Engine/Processing/commandparser.cpp
    Engine/Processing/commandparser.h
    Engine/Processing/controllerinterface.cpp
    Engine/Processing/controllerinterface.h
    Engine/Processing/datastreamfifo.cpp
    Engine/Processing/datastreamfifo.h
    Engine/Processing/displayundomanager.cpp
    Engine/Processing/displayundomanager.h
    Engine/Processing/fastfouriertransform.cpp
    Engine/Processing/fastfouriertransform.h
    Engine/Processing/filter.cpp
    Engine/Processing/filter.h
    Engine/Processing/impedancereader.cpp
    Engine/Processing/impedancereader.h
    Engine/Processing/matfilewriter.cpp
    Engine/Processing/matfilewriter.h
    Engine/Processing/minmax.h
    Engine/Processing/probemapdatastructures.h
    Engine/Processing/rhxdatareader.cpp
    Engine/Processing/rhxdatareader.h
    Engine/Processing/Semaphore.h
    Engine/Processing/signalsources.cpp
    Engine/Processing/signalsources.h
    Engine/Processing/softwarereferenceprocessor.cpp
    Engine/Processing/softwarereferenceprocessor.h
    Engine/Processing/stateitem.cpp
    Engine/Processing/stateitem.h
    Engine/Processing/stimparameters.cpp
    Engine/Processing/stimparameters.h
    Engine/Processing/stimparametersclipboard.cpp
    Engine/Processing/stimparametersclipboard.h
    Engine/Processing/systemstate.cpp
    Engine/Processing/systemstate.h
    Engine/Processing/tcpcommunicator.cpp
    Engine/Processing/tcpcommunicator.h
    Engine/Processing/waveformfifo.cpp
    Engine/Processing/waveformfifo.h
    Engine/Processing/xmlinterface.cpp
    Engine/Processing/xmlinterface.h
    Engine/Threads/audiothread.cpp
    Engine/Threads/audiothread.h
    Engine/Threads/savetodiskthread.cpp
    Engine/Threads/savetodiskthread.h
    Engine/Threads/tcpdataoutputthread.cpp
    Engine/Threads/tcpdataoutputthread.h
    Engine/Threads/usbdatathread.cpp
    Engine/Threads/usbdatathread.h
    Engine/Threads/waveformprocessorthread.cpp
    Engine/Threads/waveformprocessorthread.h
    PARENT_SCOPE
)