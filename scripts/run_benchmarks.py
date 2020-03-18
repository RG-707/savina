#!/usr/bin/python2

from subprocess import Popen
from subprocess import call
from time import sleep
import argparse

def run_akka_benchmark(name, nr, num_cores, n=10):
        if (num_cores < 10):
            error = open('./akka_results/akka_{}_{}_00{}.err'.format(nr, name, num_cores), 'w')
            output = open('./akka_results/akka_{}_{}_00{}.out'.format(nr, name, num_cores), 'w')
        elif (num_cores < 100):
            error = open('./akka_results/akka_{}_{}_0{}.err'.format(nr, name, num_cores), 'w')
            output = open('./akka_results/akka_{}_{}_0{}.out'.format(nr, name, num_cores), 'w')
        else:
            error = open('./akka_results/akka_{}_{}_{}.err'.format(nr, name, num_cores), 'w')
	    output = open('./akka_results/akka_{}_{}_{}.out'.format(nr, name, num_cores), 'w')


	run_cmd = 'java -cp ../target/savina-0.0.1-SNAPSHOT-jar-with-dependencies.jar edu.rice.habanero.benchmarks.{}'.format(name)
	p = Popen(run_cmd, shell=True, universal_newlines=True, stdout=output, stderr=error)
	p.wait()


def run_caf_benchmark(name, num_cores, n=10):
    if (num_cores < 10):
        error = open('./caf_results/caf_{}_00{}.err'.format(name, num_cores), 'w')
	output = open('./caf_results/caf_{}_00{}.out'.format(name, num_cores), 'w')
    elif (num_cores < 100):
        error = open('./caf_results/caf_{}_0{}.err'.format(name, num_cores), 'w')
	output = open('./caf_results/caf_{}_0{}.out'.format(name, num_cores), 'w')
    else:
        error = open('./caf_results/caf_{}_{}.err'.format(name, num_cores), 'w')
	output = open('./caf_results/caf_{}_{}.out'.format(name, num_cores), 'w')

    run_cmd = '../build/bin/caf_{} --iterations={}'.format(name, n)
    p = Popen(run_cmd, shell=True, universal_newlines=True, stdout=output, stderr=error)
    p.wait()

	

def run_all(num_cores, runs):
        
	run_caf_benchmark('01_pingpong', num_cores, runs)
	run_akka_benchmark('pingpong.PingPongAkkaActorBenchmark', '01', num_cores, runs)
	run_caf_benchmark('02_count', num_cores, runs)
	run_akka_benchmark('count.CountingAkkaActorBenchmark', '02', num_cores, runs)
	run_caf_benchmark('03_fjthrput', num_cores, runs)
	run_akka_benchmark('fjthrput.ThroughputAkkaActorBenchmark', '03', num_cores, runs)
	run_caf_benchmark('04_fjcreate', num_cores, runs)
	run_akka_benchmark('fjcreate.ForkJoinAkkaActorBenchmark', '04', num_cores, runs)
	run_caf_benchmark('05_threadring', num_cores, runs)
	run_akka_benchmark('threadring.ThreadRingAkkaActorBenchmark', '05', num_cores, runs)
	run_caf_benchmark('06_chameneos', num_cores, runs)
	run_akka_benchmark('chameneos.ChameneosAkkaActorBenchmark', '06', num_cores, runs)
	run_caf_benchmark('07_big', num_cores, runs)
	run_akka_benchmark('big.BigAkkaActorBenchmark', '07', num_cores, runs)
	run_caf_benchmark('08_concdict', num_cores, runs)
	run_akka_benchmark('concdict.DictionaryAkkaActorBenchmark', '08', num_cores, runs)
	run_caf_benchmark('09_concsll', num_cores, runs)
	run_akka_benchmark('concsll.SortedListAkkaActorBenchmark', '09', num_cores, runs)
	run_caf_benchmark('10_bndbuffer', num_cores, runs)
	run_akka_benchmark('bndbuffer.ProdConsAkkaActorBenchmark', 10, num_cores, runs)
	run_caf_benchmark('11_philosopher', num_cores, runs)
	run_akka_benchmark('philosopher.PhilosopherAkkaActorBenchmark', 11, num_cores, runs)
	run_caf_benchmark('12_barber', num_cores, runs)
	run_akka_benchmark('barber.SleepingBarberAkkaActorBenchmark', 12, num_cores, runs)
	run_caf_benchmark('13_cigsmok', num_cores, runs)
	run_akka_benchmark('cigsmok.CigaretteSmokerAkkaActorBenchmark', 13, num_cores, runs)
	#run_caf_benchmark('14_logmap_become_unbecome_fast', num_cores, runs)
	run_akka_benchmark('logmap.LogisticMapAkkaAwaitActorBenchmark', 14, num_cores, runs)
	#run_caf_benchmark('14_logmap_become_unbecome_slow', num_cores, runs)
	#run_caf_benchmark('14_logmap_request_await_high_timeout', num_cores, runs)
	run_caf_benchmark('14_logmap_request_await_infinite', num_cores, runs)
	#run_caf_benchmark('15_banking_become_unbecome_fast', num_cores, runs)
	run_akka_benchmark('banking.BankingAkkaAwaitActorBenchmark', 15, num_cores, runs)
	#run_caf_benchmark('15_banking_become_unbecome_slow', num_cores, runs)
	#run_caf_benchmark('15_banking_request_await_high_timeout', num_cores, runs)
	run_caf_benchmark('15_banking_request_await_infinite', num_cores, runs)
	#run_caf_benchmark('15_banking_request_then_high_timeout', num_cores, runs)
	#run_caf_benchmark('15_banking_request_then_infinite', num_cores, runs)
	run_caf_benchmark('16_radixsort', num_cores, runs)
	run_akka_benchmark('radixsort.RadixSortAkkaActorBenchmark', 16, num_cores, runs)
	run_caf_benchmark('18_sieve', num_cores, runs)
	run_akka_benchmark('sieve.SieveAkkaActorBenchmark', 18, num_cores, runs)
	run_caf_benchmark('19_uct', num_cores, runs)
	run_akka_benchmark('utc.UctScalazRelaxAkkaActorBenchmark', 19, num_cores, runs)
	run_caf_benchmark('20_facloc', num_cores, runs)
	run_akka_benchmark('facloc.FacilityLocationAkkaActorBenchmark', 20, num_cores, runs)
	run_caf_benchmark('21_trapezoid', num_cores, runs)
	run_akka_benchmark('trapezoid.TrapezoidalAkkaActorBenchmark', 21, num_cores, runs)
	#run_caf_benchmark('22_piprecision', num_cores, runs)
	#run_akka_benchmark('piprecision.PiPrecisionAkkaActorBenchmark', 22, num_cores, runs)
	run_caf_benchmark('23_recmatmul', num_cores, runs)
	run_akka_benchmark('recmatmul.MatMulAkkaActorBenchmark', 23, num_cores, runs)
	run_caf_benchmark('24_quicksort', num_cores, runs)
	run_akka_benchmark('quicksort.QuickSortAkkaActorBenchmark', 24, num_cores, runs)
	run_caf_benchmark('25_apsp', num_cores, runs)
        run_akka_benchmark('apsp.ApspAkkaActorBenchmark', 25, num_cores, runs)
	run_caf_benchmark('27_astar', num_cores, runs)
	#run_akka_benchmark('sor.SucOverRelaxAkkaActorBenchmark', num_cores, runs)
	run_akka_benchmark('astar.GuidedSearchAkkaActorBenchmark', 27, num_cores, runs)
	run_caf_benchmark('28_nqueenk', num_cores, runs)
	run_akka_benchmark('nqueenk.NQueensAkkaActorBenchmark', 28, num_cores, runs)
	run_caf_benchmark('29_fib', num_cores, runs)
	run_akka_benchmark('fib.FibonacciAkkaActorBenchmark', 29, num_cores, runs)
	run_caf_benchmark('30_bitonicsort', num_cores, runs)
	run_akka_benchmark('bitonicsort.BitonicSortAkkaActorBenchmark', 30, num_cores, runs)
	#run_akka_benchmark('filterbank.FilterBankAkkaActorBenchmark', num_cores, runs)


def main():
	parser = argparse.ArgumentParser(description='Akka benchmarks')
	parser.add_argument('-r', '--runs',  help='set number of runs	(10)', 
			    type=int, default=10)
	args = vars(parser.parse_args())
	runs = args['runs']
        for i in range(4,132,4):
            cmd = './activate_cores {}'.format(i)
            call(cmd, shell=True)
            sleep(1)
            run_all(i, runs)



if __name__ == '__main__':
	main()

