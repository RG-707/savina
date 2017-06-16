package edu.rice.habanero.benchmarks.apsp;

import edu.rice.habanero.benchmarks.BenchmarkRunner;
import edu.rice.habanero.benchmarks.CliArgumentParser;

/**
 * @author <a href="http://shams.web.rice.edu/">Shams Imam</a> (shams@rice.edu)
 */
public final class ApspConfig {

    protected static int N = 300;
    protected static int B = 50;
    protected static int W = 100;
    protected static boolean debug = false;

    protected static void parseArgs(final String[] args) {
        CliArgumentParser ap = new CliArgumentParser(args);
        N = ap.getValue("-n", N);
        B = ap.getValue("-b", B);
        W = ap.getValue("-w", W);
        debug = ap.getValue(new String[]{"--debug", "--verbose"}, debug);
    }

    protected static void printArgs() {
        System.out.printf(BenchmarkRunner.argOutputFormat, "N (num workers)", N);
        System.out.printf(BenchmarkRunner.argOutputFormat, "B (block size)", B);
        System.out.printf(BenchmarkRunner.argOutputFormat, "W (max edge weight)", W);
        System.out.printf(BenchmarkRunner.argOutputFormat, "debug", debug);
    }
}
