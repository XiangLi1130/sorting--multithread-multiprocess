#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <pthread.h>
#include <limits.h>
#include <mutex>
using namespace std;
std::mutex mtx;

struct two_pipe {
	int p_read;
	int p_write;
	//int size;
};
struct whole_pipe{
	int up_pipe[2];
	int down_pipe[2];
};
void bubblesort(vector<long long>& arr, int length);
void merge(vector<vector<long long>> parts, vector<long long> & res, int total);
void* bubblesort_th(void* arr);
void* bubblesort_t(void* pipes);

int main(int argc, char *argv[]) {
	int processNum = 2; //initialize
	int threadNum = 2; //initialize
	if (argc <= 3) { //the argument should be at least "./mysort -n num filename"
		fprintf(stderr, "full name: Xiang Li\n");
		fprintf(stderr, "seas login: xiangli3\n");
		return -1;
	}
	int value;
	//the boolean determines using process or thread
	bool process = true;
	int c;
	while ((value = getopt(argc, argv, "tn:")) != -1) {
		switch (value) {
		case 'n':
			c = atoi(optarg);
			if (c < 1) {
				fprintf(stderr, "there should be at least one process or thread \n");
				return -1;
			}
			break;
		case 't':
			process = false;
			if (threadNum < 1) {
				fprintf(stderr, "there should be at least one thread\n");
				return -1;
			}
			break;
		default:
			fprintf(stderr, "the argument passed in is illegal\n");
			return -1;
		}

	}
	if(process) {
		processNum = c;
	}else{
		threadNum = c;
	}
	int file_start_index = optind;
	std::vector<long long> allnum; // vector used to store all the input
	pid_t pids[processNum];
	for (int i = file_start_index; i < argc; i++) {
		ifstream file(argv[i]);
		long long n;
		while (file >> n) {
			allnum.push_back(n);
		}
	}
	int total = allnum.size();
	//If the user sets N=1, the program should sort the numbers directly and
	//not fork any subprocesses.
	if ((process && processNum == 1)||(!process && threadNum == 1)) {
		bubblesort(allnum, total);
		for (int i = 0; i < total; i++) {
			cout << allnum[i] << endl;
		}
		return 0;
	}
	int average, remain;
	if (process) {
		average = total / processNum;
		remain = total % processNum;
	} else {
		average = total / threadNum;
		remain = total % threadNum;
	}
	//store the input for different process
	vector<vector<long long>> separate;
	int count = 1;
	//separate the input which can be sent to different child process
	if (process) {
		for (int i = 0; i < processNum; i++) {
			vector<long long> sub;
			for (int j = (count - 1) * average; j < count * average; j++) {
				sub.push_back(allnum[j]);
			}
			if (i == processNum - 1 && remain > 0) {
				for (int k = total - remain; k < total; k++) {
					sub.push_back(allnum[k]);
				}
			}
			count++;
			separate.push_back(sub);
		}
	} else {
		for (int i = 0; i < threadNum; i++) {
			vector<long long> sub;
			for (int j = (count - 1) * average; j < count * average; j++) {
				sub.push_back(allnum[j]);
			}
			if (i == (threadNum - 1) && remain > 0) {
				for (int k = total - remain; k < total; k++) {
					sub.push_back(allnum[k]);
				}
			}
			count++;
			separate.push_back(sub);
		}
	}
	/*the process part*/
	if (process) {
		int p1[processNum][2];	//all the up pipes
		int p2[processNum][2]; //all the down pipes
		for (int i = 0; i < processNum; i++) {
			pipe(p1[i]);
			pipe(p2[i]);
		}
		int parent = 1;
		int num = 0; //keep track of the child number
		for (int i = 0; i < processNum; i++) {
			if ((pids[i] = fork()) < 0) {
				fprintf(stderr, "fork error");
				return -1;
			} else if (pids[i] == 0) {
				parent = 0;
				break; // if in the child process, break to loop;
			}
			num++;
		}
		if (parent == 1) { // is in the parent process
			for (int i = 0; i < separate.size(); i++) {
				close(p2[i][0]);
				for (int j = 0; j < separate[i].size(); j++) {
					//write each vector to different pipe
					write(p2[i][1], &separate[i][j], sizeof(separate[i][j]));
				}
			}

			//read from the pipe with sorted vectors
			vector<vector<long long> > before_merge;
			for (int i = 0; i < processNum; i++) {
				vector<long long> temp;
				close(p1[i][1]);
				long long r;
				for (int k = 0; k < separate[i].size(); k++) {
					read(p1[i][0], &r, sizeof(r));
					temp.push_back(r);
				}
				before_merge.push_back(temp);
			}

			for (int i = 0; i < processNum; i++) {
				waitpid(pids[i], NULL, 0);
			}
			//merge the vectors
			vector<long long> final_result;
			merge(before_merge, final_result, total);
			for (int i = 0; i < total; i++) {
				cout << final_result[i] << endl;
			}
		} else if (parent == 0) { //in the child process
			close(p2[num][1]);
			vector<long long> part;
			long long r;
			for (int k = 0; k < separate[num].size(); k++) {
				read(p2[num][0], &r, sizeof(r));
				part.push_back(r);
			}
			bubblesort(part, part.size());
			close(p1[num][0]);
			for (int j = 0; j < part.size(); j++) {
				write(p1[num][1], &part[j], sizeof(part[j]));
			}
		}
	}
	/*the thread part*/
	else {
		pthread_t threads[threadNum];
		whole_pipe pipess[threadNum];
		for (int i = 0; i < threadNum; i++) {
			pipe(pipess[i].up_pipe);
			pipe(pipess[i].down_pipe);
		}
		for (int i = 0; i < threadNum; i++) {
			int f = pthread_create(&threads[i], NULL, bubblesort_t, &pipess[i]);
		}

		for(int i = 0; i < threadNum; i++){
			for (int j = 0; j < separate[i].size(); j++) {
				write (pipess[i].down_pipe[1],&separate[i][j], sizeof(separate[i][j]));
			}
			long long flag = EOF;
			write(pipess[i].down_pipe[1],&flag,sizeof(flag));
		}

		vector<vector<long long> > before_merge;
		for (int i = 0; i < threadNum; i++) {
			vector<long long> temp;
			long long r;
			read(pipess[i].up_pipe[0], &r, sizeof(r));
			while (r != EOF) {
				temp.push_back(r);
				read(pipess[i].up_pipe[0], &r, sizeof(r));
			}
			before_merge.push_back(temp);
		}
		for (int i = 0; i < threadNum; i++) {
			pthread_join(threads[i], NULL);
		}

		vector<long long> final_result;
		merge(before_merge, final_result, total);
		for (int i = 0; i < final_result.size(); i++) {
			cout << final_result[i] << endl;
		}
	}
	return 0;
}

void bubblesort(vector<long long>& arr, int length) {
	for (int i = 0; i < length - 1; i++) {
		for (int j = 0; j < length - 1 - i; j++) {
			if (arr[j] > arr[j + 1]) {
				long long temp = arr[j];
				arr[j] = arr[j + 1];
				arr[j + 1] = temp;
			}
		}
	}
}

void* bubblesort_th(void* arr) {
	vector<long long>* arr1 = (vector<long long>*) arr;
	for (int i = 0; i < (*arr1).size() - 1; i++) {
		for (int j = 0; j < (*arr1).size() - 1 - i; j++) {
			if ((*arr1)[j] > (*arr1)[j + 1]) {
				long long temp = (*arr1)[j];
				(*arr1)[j] = (*arr1)[j + 1];
				(*arr1)[j + 1] = temp;
			}
		}
	}
}

void* bubblesort_t(void* pipes) {
	whole_pipe* pp = (whole_pipe*)pipes;
	vector<long long> sub;
	long long r;
    read((pp->down_pipe[0]), &r, sizeof(r));
    while(r != EOF){
    	sub.push_back(r);
    	read((pp->down_pipe[0]), &r, sizeof(r));
   }
    bubblesort(sub,sub.size());
    for (int j = 0; j < sub.size(); j++) {
       write((pp->up_pipe[1]), &sub[j], sizeof(sub[j]));
    }
    long long flag = EOF;
    write((pp->up_pipe[1]), &flag, sizeof(flag));
}

void merge(vector<vector<long long>> parts, vector<long long> & res,
		int total) {
	int count = parts.size();
	int index[count];
	for (int i = 0; i < count; i++) {
		index[i] = 0;
	}
	for (int i = 0; i < total; i++) {
		int min_index = 0;
		long long min = LONG_LONG_MAX;
		for (int j = 0; j < count; j++) {
			// at the end of the vector, no element left in that vector
			if (index[j] >= parts[j].size())
				continue;
			if (parts[j][index[j]] < min) {
				min = parts[j][index[j]];
				min_index = j;
			}
		}
		res.push_back(min);
		index[min_index]++;
	}
}

