# Makefile for Project64
# Written by Ethan "flibitijibibo" Lee

# System information
UNAME = $(shell uname)
ARCH = $(shell uname -m)

# Compiler
ifeq ($(UNAME), Darwin)
	COMPILER = clang++
else
	COMPILER = g++
endif

# Compiler and flags
COMPILER += g++ -O2

# Dynamic libraries
DEPENDENCIES = -lSDL # Just filler until we see the deps...

# Source directories
PJ64SRCDIR = .

# Parameters
INCLUDES = -I. # Just filler until we see the deps...
DEFINES = -DFLIBIT_RULES # Just filler until we see some ifdefs...

# Executable name
ifeq ($(UNAME), Darwin)
	EXECUTABLE = osx/project64.osx
endif
ifeq ($(UNAME), Linux)
	ifeq ($(ARCH), x86_64)
		EXECUTABLE = x86_64/project64.x86_64
	else
		EXECUTABLE = x86/project64.x86
	endif
endif

# Source lists
PJ64SRC = \
	$(PJ64SRCDIR)/BreakPoints.cpp \
	$(PJ64SRCDIR)/BreakPoints.cpp \
	$(PJ64SRCDIR)/Cheat.cpp \
	$(PJ64SRCDIR)/CPUAnalysis.cpp \
	$(PJ64SRCDIR)/Cpu.cpp \
	$(PJ64SRCDIR)/CPULog.cpp \
	$(PJ64SRCDIR)/Dma.cpp \
	$(PJ64SRCDIR)/Eeprom.cpp \
	$(PJ64SRCDIR)/Exception.cpp \
	$(PJ64SRCDIR)/FlashRam.cpp \
	$(PJ64SRCDIR)/InterpreterCPU.cpp \
	$(PJ64SRCDIR)/InterpreterOps.cpp \
	$(PJ64SRCDIR)/Language.cpp \
	$(PJ64SRCDIR)/Logging.cpp \
	$(PJ64SRCDIR)/main2.cpp \
	$(PJ64SRCDIR)/Main.cpp \
	$(PJ64SRCDIR)/Mapping.cpp \
	$(PJ64SRCDIR)/Memory.cpp \
	$(PJ64SRCDIR)/Mempak.cpp \
	$(PJ64SRCDIR)/MemTest.cpp \
	$(PJ64SRCDIR)/Pif.cpp \
	$(PJ64SRCDIR)/Plugin.cpp \
	$(PJ64SRCDIR)/Profiling.cpp \
	$(PJ64SRCDIR)/r4300iCommands.cpp \
	$(PJ64SRCDIR)/r4300iMemory.cpp \
	$(PJ64SRCDIR)/r4300iRegisters.cpp \
	$(PJ64SRCDIR)/RecompilerCPU.cpp \
	$(PJ64SRCDIR)/RecompilerFpuOps.cpp \
	$(PJ64SRCDIR)/RecompilerOps.cpp \
	$(PJ64SRCDIR)/Registers.cpp \
	$(PJ64SRCDIR)/RomBrowser.cpp \
	$(PJ64SRCDIR)/Rom.cpp \
	$(PJ64SRCDIR)/SettingsApi.cpp \
	$(PJ64SRCDIR)/Settings.cpp \
	$(PJ64SRCDIR)/Sram.cpp \
	$(PJ64SRCDIR)/SyncCPU.cpp \
	$(PJ64SRCDIR)/Tlb.cpp \
	$(PJ64SRCDIR)/TLBDisplay.cpp \
	$(PJ64SRCDIR)/Win32Timer.cpp \
	$(PJ64SRCDIR)/X86.cpp \
	$(PJ64SRCDIR)/x86fpu.cpp \
	$(PJ64SRCDIR)/X86rsp.cpp

# Object code lists
PJ64OBJ = $(PJ64SRC:%.cpp=%.o)

# Targets
all: $(PJ64OBJ)
	$(COMPILER) $(PJ64OBJ) -o $(EXECUTABLE) $(DEPENDENCIES)

$(PJ64SRCDIR)/%.o: $(PJ64SRCDIR)/%.cpp
	$(COMPILER) -c -o $@ $< $(INCLUDES) $(DEFINES)

clean:
	rm $(PJ64OBJ) $(EXECUTABLE)
