##
##

BINARIES = monitor

CXXFLAGS_STANDARD = -g
CXXFLAGS_FAM =
CXXFLAGS = ${CXXFLAGS_STANDARD} ${CXXFLAGS_FAM}

LDFLAGS_STANDARD = -g
LDFLAGS_FAM = -lfam
LDFLAGS = ${LDFLAGS_STANDARD} ${LDFLAGS_FAM}

all: monitor


monitor: monitor.cc
	${CXX} ${CXXFLAGS} -o monitor monitor.cc ${LDFLAGS}


debug: monitor.cc
	${CXX} ${CXXFLAGS} -g -DDEBUG -o monitor monitor.cc ${LDFLAGS}

clean:
	-rm -f ${BINARIES}

