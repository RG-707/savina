package edu.rice.habanero.benchmarks.bitonicsort;

import edu.rice.habanero.benchmarks.BenchmarkRunner;
import edu.rice.habanero.benchmarks.CliArgumentParser;

/**
 * @author <a href="http://shams.web.rice.edu/">Shams Imam</a> (shams@rice.edu)
 */
public final class BitonicSortConfig {

    protected static int N = 4_096; // data size, must be power of 2
    protected static long M = 1L << 60; // max value
    protected static long S = 2_048; // seed for random number generator
    protected static boolean debug = false;

    protected static void parseArgs(final String[] args) {
        CliArgumentParser ap = new CliArgumentParser(args);
        N = ap.getValue("-n", N);
        M = ap.getValue("-m", M);
        S = ap.getValue("-s", S);
        debug = ap.getValue(new String[]{"--debug", "--verbose"}, debug);
    }

    protected static void printArgs() {
        System.out.printf(BenchmarkRunner.argOutputFormat, "N (num values)", N);
        System.out.printf(BenchmarkRunner.argOutputFormat, "M (max value)", M);
        System.out.printf(BenchmarkRunner.argOutputFormat, "S (seed)", S);
        System.out.printf(BenchmarkRunner.argOutputFormat, "debug", debug);
    }
}
