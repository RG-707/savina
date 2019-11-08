#!/usr/bin/python

from subprocess import Popen

def run_caf_benchmark(name, num_cores, n=100):
	error = open('./caf_results/caf_{}_{}.err'.format(name, num_cores), 'w')
	output = open('./caf_results/caf_{}_{}.out'.format(name, num_cores), 'w')

	run_cmd = '../build/bin/caf_{} --iterations={}'.format(name, n)
	p = Popen(run_cmd, shell=True, universal_newlines=True, stdout=output, stderr=error)
	p.wait()
	

def run_all(num_cores, runs):
	run_caf_benchmark('01_pingpong', num_cores, runs)
	run_caf_benchmark('02_count', num_cores, runs)
	run_caf_benchmark('03_fjthrput', num_cores, runs)
	run_caf_benchmark('04_fjcreate', num_cores, runs)
	run_caf_benchmark('05_threadring', num_cores, runs)
	run_caf_benchmark('06_chameneos', num_cores, runs)
	run_caf_benchmark('07_big', num_cores, runs)
	run_caf_benchmark('08_concdict', num_cores, runs)
	run_caf_benchmark('09_concsll', num_cores, runs)
	run_caf_benchmark('10_bndbuffer', num_cores, runs)
	run_caf_benchmark('11_philosopher', num_cores, runs)
	run_caf_benchmark('12_barber', num_cores, runs)
	run_caf_benchmark('13_cigsmok', num_cores, runs)
	run_caf_benchmark('14_logmap_become_unbecome_fast', num_cores, runs)
	run_caf_benchmark('14_logmap_become_unbecome_slow', num_cores, runs)
	run_caf_benchmark('14_logmap_request_await_high_timeout', num_cores, runs)
	run_caf_benchmark('14_logmap_request_await_infinite', num_cores, runs)
	run_caf_benchmark('15_banking_become_unbecome_fast', num_cores, runs)
	run_caf_benchmark('15_banking_become_unbecome_slow', num_cores, runs)
	run_caf_benchmark('15_banking_request_await_high_timeout', num_cores, runs)
	run_caf_benchmark('15_banking_request_await_infinite', num_cores, runs)
	run_caf_benchmark('15_banking_request_then_high_timeout', num_cores, runs)
	run_caf_benchmark('15_banking_request_then_infinite', num_cores, runs)
	run_caf_benchmark('16_radixsort', num_cores, runs)
	run_caf_benchmark('18_sieve', num_cores, runs)
	run_caf_benchmark('19_uct', num_cores, runs)
	run_caf_benchmark('20_facloc', num_cores, runs)
	run_caf_benchmark('21_trapezoid', num_cores, runs)
	#run_caf_benchmark('22_piprecision', num_cores, runs)
	run_caf_benchmark('23_recmatmul', num_cores, runs)
	run_caf_benchmark('24_quicksort', num_cores, runs)
	run_caf_benchmark('25_apsp', num_cores, runs)
	run_caf_benchmark('27_astar', num_cores, runs)
	run_caf_benchmark('28_nqueenk', num_cores, runs)
	run_caf_benchmark('29_fib', num_cores, runs)
	run_caf_benchmark('30_bitonicsort', num_cores, runs)


def main():
	parser = argparse.ArgumentParser(description='CAF benchmarks')
	parser.add_argument('-r', '--runs',  help='set number of runs	(10)', 
			    type=int, default=10)
	parser.add_argument('-c', '--cores',  help='set number of cores	(4)',
			    type=int, default=4)
	args = vars(parser.parse_args())
	num_cores = args['cores']
	runs = args['runs']
	run_all(num_cores, runs)


if __name__ == '__main__':
	main()

