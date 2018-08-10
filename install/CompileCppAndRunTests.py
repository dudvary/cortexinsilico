#! /usr/bin/python3

from subprocess import check_call
from subprocess import Popen, PIPE
import os
import sys


def compileCppAndRunUnitTests(src_dir, build_dir):
    print("[*] Compiling c++ programs")

    computeSynapses_pro_file = os.path.join(src_dir, "computeSynapses/computeSynapses.pro")
    computeSynapses_build_dir = os.path.join(build_dir, "computeSynapses")
    os.makedirs(computeSynapses_build_dir, exist_ok=True)
    check_call(["qmake", computeSynapses_pro_file], cwd=computeSynapses_build_dir)
    check_call(["make", "all"], cwd=computeSynapses_build_dir)

    compareData_pro_file = os.path.join(src_dir, "compareData/compareData.pro")
    compareData_build_dir = os.path.join(build_dir, "compareData")
    os.makedirs(compareData_build_dir, exist_ok=True)
    check_call(["qmake", compareData_pro_file], cwd=compareData_build_dir)
    check_call(["make", "all"], cwd=compareData_build_dir)

    networkSimulator_pro_file = os.path.join(src_dir, "networkSimulator/networkSimulator.pro")
    networkSimulator_build_dir = os.path.join(build_dir, "networkSimulator")
    os.makedirs(networkSimulator_build_dir, exist_ok=True)
    check_call(["qmake", networkSimulator_pro_file], cwd=networkSimulator_build_dir)
    check_call(["make", "all"], cwd=networkSimulator_build_dir)

    convertAxon_pro_file = os.path.join(src_dir, "convertAxonRedundancyMap/convertAxonRedundancyMap.pro")
    convertAxon_build_dir = os.path.join(build_dir, "convertAxonRedundancyMap")
    os.makedirs(convertAxon_build_dir, exist_ok=True)
    check_call(["qmake", convertAxon_pro_file], cwd=convertAxon_build_dir)
    check_call(["make", "all"], cwd=convertAxon_build_dir)

    convertSparse_pro_file = os.path.join(src_dir, "convertSparseField/convertSparseField.pro")
    convertSparse_build_dir = os.path.join(build_dir, "convertSparseField")
    os.makedirs(convertSparse_build_dir, exist_ok=True)
    check_call(["qmake", convertSparse_pro_file], cwd=convertSparse_build_dir)
    check_call(["make", "all"], cwd=convertSparse_build_dir)

    computeStatistic_pro_file = os.path.join(src_dir, "computeStatistic/computeStatistic.pro")
    computeStatistic_build_dir = os.path.join(build_dir, "computeStatistic")
    os.makedirs(computeStatistic_build_dir, exist_ok=True)
    check_call(["qmake", computeStatistic_pro_file], cwd=computeStatistic_build_dir)
    check_call(["make", "all"], cwd=computeStatistic_build_dir)

    process3DQuery_pro_file = os.path.join(src_dir, "processCIS3DQuery/processCIS3DQuery.pro")
    process3DQuery_build_dir = os.path.join(build_dir, "processCIS3DQuery")
    os.makedirs(process3DQuery_build_dir, exist_ok=True)
    check_call(["qmake", process3DQuery_pro_file], cwd=process3DQuery_build_dir)
    check_call(["make", "all", "-j", "4"], cwd=process3DQuery_build_dir)

    inputmapper_pro_file = os.path.join(src_dir, "inputmapper/inputmapper.pro")
    inputmapper_build_dir = os.path.join(build_dir, "inputmapper")
    os.makedirs(inputmapper_build_dir, exist_ok=True)
    check_call(["qmake", inputmapper_pro_file], cwd=inputmapper_build_dir)
    check_call(["make", "all", "-j", "4"], cwd=inputmapper_build_dir)

    convert_pro_file = os.path.join(src_dir, "convertInnervationToCSV/convertInnervationToCSV.pro")
    convert_build_dir = os.path.join(build_dir, "convert")
    os.makedirs(convert_build_dir, exist_ok=True)
    check_call(["qmake", convert_pro_file], cwd=convert_build_dir)
    check_call(["make", "all", "-j", "4"], cwd=convert_build_dir)

    test_pro_file = os.path.join(src_dir, "test/runTests.pro")
    test_build_dir = os.path.join(build_dir, "test")
    os.makedirs(test_build_dir, exist_ok=True)
    check_call(["qmake", test_pro_file], cwd=test_build_dir)
    check_call(["make", "all", "-j", "4"], cwd=test_build_dir)

    print("[*] Running unit tests")
    check_call(["./release/runTests"], cwd=test_build_dir)
    print("\n")


def installAptPackage(package):
    try:
        ps = Popen(('dpkg', '-s', package), stdout=PIPE)
        check_call(('grep', 'Status'), stdin=ps.stdout)
        ps.wait()
        print("[*] {0} already available".format(package))
    except:
        print("[*] Installing {0}".format(package))
        check_call(["sudo", "apt-get", "install", "--yes", package])


def printUsageAndExit():
    print("Usage:")
    print("cd cortexinsilico-buildingbrains")
    print("python3 install/CompileAndRunTests.py")
    sys.exit(1)


repo_root = os.getcwd()
src_dir = os.path.join(repo_root, 'src')
build_dir = os.path.join(repo_root, 'build')

if not os.path.isdir(src_dir):
    print("Cannot find directory 'src'. Are you in the correct directory?")
    printUsageAndExit()

os.makedirs(build_dir, exist_ok=True)

installAptPackage('gcc')
installAptPackage('qt5-default')
installAptPackage('qt5-qmake')

compileCppAndRunUnitTests(src_dir, build_dir)

print("Done.")
