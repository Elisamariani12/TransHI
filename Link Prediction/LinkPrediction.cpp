#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <array>
#include <iostream>
#include <string_view>

using namespace std;

long relationTotal;
long entityTotal;
long Threads = 12;
long dimensionR = 100;
long dimension = 100;
long binaryFlag = 0;

float *entityVec, *relationVec;
long testTotal, tripleTotal, trainTotal, validTotal;

struct Triple {
    long h, r, t;
    long label;
};

struct cmp_head {
    bool operator()(const Triple &a, const Triple &b) {
        return (a.h < b.h)||(a.h == b.h && a.r < b.r)||(a.h == b.h && a.r == b.r && a.t < b.t);
    }
};

Triple *testList, *tripleList;
string initPath = "";
string inPath = "";
string outPath = "";
string note = "";
int nntotal[5];
int head_lef[10000];
int head_rig[10000];
int tail_lef[10000];
int tail_rig[10000];
int head_type[1000000];
int tail_type[1000000];

const string typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
int typeOf_id;

void init() {

	//carico le relazioni per il confronto
	string stmp;
    ifstream rel_file(inPath + "relation2id.txt");
    getline(rel_file,stmp);
    while (getline(rel_file, stmp)) {
        string::size_type pos=stmp.find('\t',0);
        string rel = stmp.substr(0,pos);
        int id = atoi(stmp.substr(pos+1).c_str());

        if(rel == typeOf){
        	typeOf_id = id;
        }
    }
    printf("ID TypeOf: %d\n", typeOf_id);

    

    rel_file.close();



    FILE *fin;
    long tmp, h, r, t, label;



    fin = fopen((inPath + "relation2id.txt").c_str(), "r");


    tmp = fscanf(fin, "%ld", &relationTotal);
    fclose(fin);
    printf("relation2id loaded \n");



    relationVec = (float *)calloc(relationTotal * dimensionR, sizeof(float));

    fin = fopen((inPath + "entity2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%ld", &entityTotal);
    fclose(fin);
    entityVec = (float *)calloc(entityTotal * dimension, sizeof(float));
    printf("entity2id loaded \n");


    FILE* f_kb1 = fopen((inPath + "test2id_Consistent.txt").c_str(),"r");
    FILE* f_kb2 = fopen((inPath + "train2id_Consistent_withAugmentation.txt").c_str(),"r");
    FILE* f_kb3 = fopen((inPath + "valid2id_Consistent.txt").c_str(),"r");

    tmp = fscanf(f_kb1, "%ld", &testTotal);   
    tmp = fscanf(f_kb2, "%ld", &trainTotal);  
    tmp = fscanf(f_kb3, "%ld", &validTotal); 

    cout<<"test total is:"<<testTotal<<"\n";

    tripleTotal = testTotal + trainTotal + validTotal;
    testList = (Triple *)calloc(testTotal, sizeof(Triple));
    tripleList = (Triple *)calloc(tripleTotal, sizeof(Triple));
    memset(nntotal, 0, sizeof(nntotal));
    for (long i = 0; i < testTotal; i++) {
    	tmp = fscanf(f_kb1, "%ld", &label);
        tmp = fscanf(f_kb1, "%ld", &h);
        tmp = fscanf(f_kb1, "%ld", &t);
        tmp = fscanf(f_kb1, "%ld", &r);
        label++;
        nntotal[label]++;
        testList[i].label = label;
        testList[i].h = h;
        testList[i].t = t;
        testList[i].r = r;
        tripleList[i].h = h;
        tripleList[i].t = t;
        tripleList[i].r = r;
    }


    for (long i = 0; i < trainTotal; i++) {
        tmp = fscanf(f_kb2, "%ld", &h);
        tmp = fscanf(f_kb2, "%ld", &t);
        tmp = fscanf(f_kb2, "%ld", &r);
        tripleList[i + testTotal].h = h;
        tripleList[i + testTotal].t = t;
        tripleList[i + testTotal].r = r;
    }

    for (long i = 0; i < validTotal; i++) {
        tmp = fscanf(f_kb3, "%ld", &h);
        tmp = fscanf(f_kb3, "%ld", &t);
        tmp = fscanf(f_kb3, "%ld", &r);
        tripleList[i + testTotal + trainTotal].h = h;
        tripleList[i + testTotal + trainTotal].t = t;
        tripleList[i + testTotal + trainTotal].r = r;
    }


    fclose(f_kb1);
    fclose(f_kb2);
    fclose(f_kb3);

    sort(tripleList, tripleList + tripleTotal, cmp_head());

    long total_lef = 0;
    long total_rig = 0;
    FILE* f_type = fopen((inPath + "type_constrain.txt").c_str(),"r");
    tmp = fscanf(f_type, "%ld", &tmp); //len(rellef))

    for (int i = 0; i < relationTotal; i++) {
        int rel, tot;
        tmp = fscanf(f_type, "%d%d", &rel, &tot); //i,len(rellef[i])
        head_lef[rel] = total_lef; //0
        for (int j = 0; j < tot; j++) {
            tmp = fscanf(f_type, "%d", &head_type[total_lef]); //j
            total_lef++;
        }
        head_rig[rel] = total_lef; //len
        sort(head_type + head_lef[rel], head_type + head_rig[rel]); 
        
        tmp = fscanf(f_type, "%d%d", &rel, &tot);
        tail_lef[rel] = total_rig;
        for (int j = 0; j < tot; j++) {
            tmp = fscanf(f_type, "%d", &tail_type[total_rig]);
            total_rig++;
        }
        tail_rig[rel] = total_rig;
        sort(tail_type + tail_lef[rel], tail_type + tail_rig[rel]);
    }

    fclose(f_type);
    printf("Init done \n");
}

void prepre_binary() {
    struct stat statbuf1;
    if (stat((initPath + "entity2vec" + note + ".bin").c_str(), &statbuf1) != -1) {
        int fd = open((initPath + "entity2vec" + note + ".bin").c_str(), O_RDONLY);
        float* entityVecTmp = (float*)mmap(NULL, statbuf1.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        memcpy(entityVec, entityVecTmp, statbuf1.st_size);
        munmap(entityVecTmp, statbuf1.st_size);
        close(fd);
    }
    struct stat statbuf2;
    if (stat((initPath + "relation2vec" + note + ".bin").c_str(), &statbuf2) != -1) {
        int fd = open((initPath + "relation2vec" + note + ".bin").c_str(), O_RDONLY);
        float* relationVecTmp =(float*)mmap(NULL, statbuf2.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        memcpy(relationVec, relationVecTmp, statbuf2.st_size);
        munmap(relationVecTmp, statbuf2.st_size);
        close(fd);
    }
}

void prepare() {
    if (binaryFlag) {
        prepre_binary();
        return;
    }
    printf("start loading embeddings \n");
    FILE *fin;
    long tmp;
    fin = fopen((initPath + "entity2vec" + note + ".vec").c_str(), "r");
    for (long i = 0; i < entityTotal; i++) {
        long last = i * dimension;
        for (long j = 0; j < dimension; j++)
            tmp = fscanf(fin, "%f", &entityVec[last + j]);
    }
    fclose(fin);
    fin = fopen((initPath + "relation2vec" + note + ".vec").c_str(), "r");
    for (long i = 0; i < relationTotal; i++) {
        long last = i * dimensionR;
        for (long j = 0; j < dimensionR; j++)
            tmp = fscanf(fin, "%f", &relationVec[last + j]);
    }

    fclose(fin);
    printf("embeddings loaded \n");
}

float calc_sum(long e1, long e2, long rel) {
    float res = 0;
    long last1 = e1 * dimension;
    long last2 = e2 * dimension;
    long lastr = rel * dimensionR;
    for (long i = 0; i < dimensionR; i++)
        res += fabs(entityVec[last1 + i] + relationVec[lastr + i] - entityVec[last2 + i]);
    return res;
}

bool find(long h, long t, long r) {
    long lef = 0;
    long rig = tripleTotal - 1;
    long mid;
    while (lef + 1 < rig) {
        long mid = (lef + rig) >> 1;
        if ((tripleList[mid]. h < h) || (tripleList[mid]. h == h && tripleList[mid]. r < r) || (tripleList[mid]. h == h && tripleList[mid]. r == r && tripleList[mid]. t < t)) lef = mid; else rig = mid;
    }
    if (tripleList[lef].h == h && tripleList[lef].r == r && tripleList[lef].t == t) return true;
    if (tripleList[rig].h == h && tripleList[rig].r == r && tripleList[rig].t == t) return true;
    return false;
}

float *l_filter_tot[6], *r_filter_tot[6], *l_tot[6], *r_tot[6];
float *l_filter_rank[6], *r_filter_rank[6], *l_rank[6], *r_rank[6];

void remove(std::vector<int> &v){
	std::vector<int>::iterator itr = v.begin();
	std::unordered_set<int> s;
	for (auto curr = v.begin(); curr != v.end(); ++curr){
		if (s.insert(*curr).second) {
			*itr++ = *curr;
		}
	}
	v.erase(itr, v.end());
}

void* testMode(void *con) {
    //ofstream output_inconsistents_head(outPath + "inconsistents_head"+ note + ".txt",std::ios_base::app);
    //ofstream output_inconsistents_tail(outPath + "inconsistents_tail"+ note + ".txt",std::ios_base::app);
    ofstream output_predicted(outPath + "predicted2id"+ note + "_bottom.txt",std::ios_base::app);
    ofstream output_not_predicted(outPath + "not_predicted2id"+ note + ".txt",std::ios_base::app);
    long id;
    id = (unsigned long long)(con);
    long lef = testTotal / (Threads) * id; //0
    long rig = testTotal / (Threads) * (id + 1) - 1; //testTotal
    if (id == Threads - 1) rig = testTotal - 1;

    for (long i = lef; i <= rig; i++) {
        long h = testList[i].h;
        long t = testList[i].t;
        long r = testList[i].r;
        long label = testList[i].label;
        float minimal = calc_sum(h, t, r);
        long l_filter_s = 0;
        long l_s = 0;
        long r_filter_s = 0;
        long r_s = 0;
        long l_filter_s_constrain = 0;
        long l_s_constrain = 0;
        long r_filter_s_constrain = 0;
        long r_s_constrain = 0;
        long type_head = head_lef[r]; 
        long type_tail = tail_lef[r];
        std::map<int, float> sort_h;
        std::map<int, float> sort_t;
        std::map<int, float> sort_not_pred_h;
        std::map<int, float> sort_not_pred_t;
        for (long j = 0; j < entityTotal; j++) {
            if (j != h) {
                float value = calc_sum(j, t, r);
                if (value < minimal) {
                    l_s += 1;
                    if (not find(j, t, r)){
                        l_filter_s += 1;
                        //output_inconsistents_head << "(" << j << ", " << t << ", " << r << "), ";
                        //output_inconsistents_head << j << " " << t << " " << r << endl;
                        sort_h[j] = value;
                    }
                }else {
                    if(not find(j, t, r)){
                        sort_not_pred_h[j]=value;
                    }
                }
                //while (head_lef[r] < head_rig[r] && head_type[head_lef[r]] < j) head_lef[r]++;
                while (type_head < head_rig[r] && head_type[type_head] < j) type_head++;
                if (type_head < head_rig[r] && head_type[type_head] == j) {
                    if (value < minimal) {
                        l_s_constrain += 1;
                        if (not find(j, t, r))
                            l_filter_s_constrain += 1;
                    }
                }
            }   
        }

	//"<" sens croissant
        std::vector<std::pair<int, float>> top_N_h(5);
        std::partial_sort_copy(sort_h.begin(),sort_h.end(),top_N_h.begin(),top_N_h.end(),[](std::pair<const int, float> const& l,std::pair<const int, float> const& r){return l.second > r.second;});
        std::vector<std::pair<int, float>> top_N_not_pred_h(2);
        std::partial_sort_copy(sort_not_pred_h.begin(),sort_not_pred_h.end(),top_N_not_pred_h.begin(),top_N_not_pred_h.end(),[](std::pair<const int, float> const& l,std::pair<const int, float> const& r){return l.second < r.second;});
        
        /*
        std::cout << "top_N_h contains:";
        std::cout << '\n';
        for (auto& it : top_N_h) {
            std::cout << it.first << ' ' << it.second << std::endl;
        }
        */

        std::vector<int> key_h;
        for(auto& it : top_N_h) {
            key_h.push_back(it.first);
        }
        /*
        for (auto& it : key_h){
            std::cout << it << std::endl; 
        }
        */
        remove(key_h);
        for (auto& it : key_h){
            //output_inconsistents_head << it << " " << t << " " << r << endl;
            output_predicted << it << " " << t << " " << r << endl;
        }


        std::vector<int> key_not_pred_h;
        for(auto& it : top_N_not_pred_h) {
            key_not_pred_h.push_back(it.first);
        }
        remove(key_not_pred_h);
        for (auto& it : key_not_pred_h){
            output_not_predicted << it << " " << t << " " << r << endl;
        }
                                   

        for (long j = 0; j < entityTotal; j++) {
            if (j != t) {
                float value = calc_sum(h, j, r);
                if (value < minimal) {
                    r_s += 1;
                    if (not find(h, j, r)){
                        r_filter_s += 1;
                        //output_inconsistents_tail << "(" << h << ", " << j << ", " << r << "), ";
                        //output_inconsistents_tail << h << " " << j << " " << r << endl;
                        sort_t[j] = value;
                    }
                }else{
                    if(not find(h, j, r)){
                        sort_not_pred_t[j]=value;
                    }
                }
                while (type_tail < tail_rig[r] && tail_type[type_tail] < j) type_tail++;
                if (type_tail < tail_rig[r] && tail_type[type_tail] == j) {
                    if (value < minimal) {
                        r_s_constrain += 1;
                        if (not find(h, j, r))
                            r_filter_s_constrain += 1;
                    }
                }
            }
        }
        std::vector<std::pair<int, float>> top_N_t(5);
        std::partial_sort_copy(sort_t.begin(),sort_t.end(),top_N_t.begin(),top_N_t.end(),[](std::pair<const int, float> const& l,std::pair<const int, float> const& r){return l.second > r.second;});

        std::vector<std::pair<int, float>> top_N_not_pred_t(2);
        std::partial_sort_copy(sort_not_pred_t.begin(),sort_not_pred_t.end(),top_N_not_pred_t.begin(),top_N_not_pred_t.end(),[](std::pair<const int, float> const& l,std::pair<const int, float> const& r){return l.second < r.second;});

        /*
        std::cout << "top_N_t contains:";
        std::cout << '\n';
        for (auto& it : top_N_t) {
            std::cout << it.first << ' ' << it.second << std::endl;
        }
        */
        std::vector<int> key_t;
        for(auto& it : top_N_t) {
            key_t.push_back(it.first);
        }
        /*
        for (auto& it : key_t){
            std::cout << it << std::endl; 
        }
        */
        remove(key_t);
        for (auto& it : key_t){
            //output_inconsistents_tail << h << " " << it << " " << r << endl;
            output_predicted << h << " " << it << " " << r << endl;
        }

        std::vector<int> key_not_pred_t;
        for(auto& it : top_N_not_pred_t) {
            key_not_pred_t.push_back(it.first);
        }
        remove(key_not_pred_t);
        for (auto& it : key_not_pred_t){
            output_not_predicted << h << " " << it << " " << r << endl;
        }

        //output_inconsistents_head << endl;
        //output_inconsistents_tail << endl;

        if (l_filter_s < 10) l_filter_tot[0][id] += 1;
        if (l_s < 10) l_tot[0][id] += 1;
        if (r_filter_s < 10) r_filter_tot[0][id] += 1;
        if (r_s < 10) r_tot[0][id] += 1;

        l_filter_rank[0][id] += l_filter_s;
        r_filter_rank[0][id] += r_filter_s;
        l_rank[0][id] += l_s;
        r_rank[0][id] += r_s;

        if (l_filter_s < 10) l_filter_tot[label][id] += 1;
        if (l_s < 10) l_tot[label][id] += 1;
        if (r_filter_s < 10) r_filter_tot[label][id] += 1;
        if (r_s < 10) r_tot[label][id] += 1;

        l_filter_rank[label][id] += l_filter_s;
        r_filter_rank[label][id] += r_filter_s;
        l_rank[label][id] += l_s;
        r_rank[label][id] += r_s;



        if (l_filter_s_constrain < 10) l_filter_tot[5][id] += 1;
        if (l_s_constrain < 10) l_tot[5][id] += 1;
        if (r_filter_s_constrain < 10) r_filter_tot[5][id] += 1;
        if (r_s_constrain < 10) r_tot[5][id] += 1;

        l_filter_rank[5][id] += l_filter_s_constrain;
        r_filter_rank[5][id] += r_filter_s_constrain;
        l_rank[5][id] += l_s_constrain;
        r_rank[5][id] += r_s_constrain;
    }
    //output_inconsistents_head.close();
    //output_inconsistents_tail.close();
    output_predicted.close();
    output_not_predicted.close();

    pthread_exit(NULL);
}

void test(void *con) {
	for (int i = 0; i <= 5; i++) {
	    l_filter_tot[i] = (float *)calloc(Threads, sizeof(float));
	    r_filter_tot[i] = (float *)calloc(Threads, sizeof(float));
	    l_tot[i] = (float *)calloc(Threads, sizeof(float));
	    r_tot[i] = (float *)calloc(Threads, sizeof(float));

	    l_filter_rank[i] = (float *)calloc(Threads, sizeof(float));
	    r_filter_rank[i] = (float *)calloc(Threads, sizeof(float));
	    l_rank[i] = (float *)calloc(Threads, sizeof(float));
	    r_rank[i] = (float *)calloc(Threads, sizeof(float));
	}

    pthread_t *pt = (pthread_t *)malloc(Threads * sizeof(pthread_t));
    for (long a = 0; a < Threads; a++)
        pthread_create(&pt[a], NULL, testMode,  (void*)a);
    for (long a = 0; a < Threads; a++)
        pthread_join(pt[a], NULL);
    free(pt);
	for (int i = 0; i <= 5; i++)
    for (long a = 1; a < Threads; a++) {
        l_filter_tot[i][a] += l_filter_tot[i][a - 1];
        r_filter_tot[i][a] += r_filter_tot[i][a - 1];
        l_tot[i][a] += l_tot[i][a - 1];
        r_tot[i][a] += r_tot[i][a - 1];

        l_filter_rank[i][a] += l_filter_rank[i][a - 1];
        r_filter_rank[i][a] += r_filter_rank[i][a - 1];
        l_rank[i][a] += l_rank[i][a - 1];
        r_rank[i][a] += r_rank[i][a - 1];
    }
	float rank=0,tot=0, rank_filt=0, tot_filt=0;
   	for (int i = 0; i <= 0; i++) {
	    printf("left %f %f\n", l_rank[i][Threads - 1] / testTotal, l_tot[i][Threads - 1] / testTotal);
	    rank += l_rank[i][Threads - 1] / testTotal;
	    tot += l_tot[i][Threads - 1] / testTotal;
	    printf("left(filter) %f %f\n", l_filter_rank[i][Threads - 1] / testTotal, l_filter_tot[i][Threads - 1] / testTotal);
	    rank_filt += l_filter_rank[i][Threads - 1] / testTotal;
	    tot_filt += l_filter_tot[i][Threads - 1] / testTotal;
	    printf("right %f %f\n", r_rank[i][Threads - 1] / testTotal, r_tot[i][Threads - 1] / testTotal);
	    rank += r_rank[i][Threads - 1] / testTotal;
	    tot += r_tot[i][Threads - 1] / testTotal;
	    printf("right(filter) %f %f\n", r_filter_rank[i][Threads - 1] / testTotal, r_filter_tot[i][Threads - 1] / testTotal);
	    rank_filt += r_filter_rank[i][Threads - 1] / testTotal;
	    tot_filt += r_filter_tot[i][Threads - 1] / testTotal;

	}
   	cout << endl;
    for (int i = 5; i <= 5; i++) {
        printf("left %f %f\n", l_rank[i][Threads - 1] / testTotal, l_tot[i][Threads - 1] / testTotal);
	    rank += l_rank[i][Threads - 1] / testTotal;
	    tot += l_tot[i][Threads - 1] / testTotal;
        printf("left(filter) %f %f\n", l_filter_rank[i][Threads - 1] / testTotal, l_filter_tot[i][Threads - 1] / testTotal);
	    rank_filt += l_filter_rank[i][Threads - 1] / testTotal;
	    tot_filt += l_filter_tot[i][Threads - 1] / testTotal;
        printf("right %f %f\n", r_rank[i][Threads - 1] / testTotal, r_tot[i][Threads - 1] / testTotal);
        rank += r_rank[i][Threads - 1] / testTotal;
		tot += r_tot[i][Threads - 1] / testTotal;
        printf("right(filter) %f %f\n", r_filter_rank[i][Threads - 1] / testTotal, r_filter_tot[i][Threads - 1] / testTotal);
	    rank_filt += r_filter_rank[i][Threads - 1] / testTotal;
	    tot_filt += r_filter_tot[i][Threads - 1] / testTotal;
    }
   	cout << endl;
	for (int i = 1; i <= 4; i++) {
	    printf("left %f %f\n", l_rank[i][Threads - 1] / nntotal[i], l_tot[i][Threads - 1] / nntotal[i]);
	    rank += l_rank[i][Threads - 1] / nntotal[i];
	    tot += l_tot[i][Threads - 1] / nntotal[i];
	    printf("left(filter) %f %f\n", l_filter_rank[i][Threads - 1] / nntotal[i], l_filter_tot[i][Threads - 1] / nntotal[i]);
	    rank_filt += l_filter_rank[i][Threads - 1] / nntotal[i];
	    tot_filt += l_filter_tot[i][Threads - 1] / nntotal[i];
	    printf("right %f %f\n", r_rank[i][Threads - 1] / nntotal[i], r_tot[i][Threads - 1] / nntotal[i]);
        rank += r_rank[i][Threads - 1] / nntotal[i];
		tot += r_tot[i][Threads - 1] / nntotal[i];
	    printf("right(filter) %f %f\n", r_filter_rank[i][Threads - 1] / nntotal[i], r_filter_tot[i][Threads - 1] / nntotal[i]);
	    rank_filt += r_filter_rank[i][Threads - 1] / nntotal[i];
	    tot_filt += r_filter_tot[i][Threads - 1] / nntotal[i];
	}

	ofstream output(outPath + "result"+ note + ".txt",std::ios_base::app);
	output << "lp_rank" << note << ".append(" << (rank/12) << ")" << endl;
	output << "lp_tot" << note << ".append(" << (tot/12) << ")" << endl;
	output << "lp_rank_fil" << note << ".append(" << (rank_filt/12) << ")" << endl;
	output << "lp_tot_fil" << note << ".append(" << (tot_filt/12) << ")" << endl;
	output.close();
	cout << "FINISH " << note << endl;

    printf("Rank %f\n", rank);
	printf("Tot %f\n", tot);
	printf("Rank(filter) %f\n", rank_filt);
	printf("Tot(filter) %f\n", tot_filt);
}

long ArgPos(char *str, long argc, char **argv) {
    long a;
    for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
        if (a == argc - 1) {
            printf("Argument missing for %s\n", str);
            exit(1);
        }
        return a;
    }
    return -1;
}

void setparameters(long argc, char **argv) {
    long i;
    if ((i = ArgPos((char *)"-size", argc, argv)) > 0) dimension = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-sizeR", argc, argv)) > 0) dimensionR = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-input", argc, argv)) > 0) inPath = argv[i + 1];
    if ((i = ArgPos((char *)"-output", argc, argv)) > 0) outPath = argv[i + 1];
    if ((i = ArgPos((char *)"-init", argc, argv)) > 0) initPath = argv[i + 1];
    if ((i = ArgPos((char *)"-thread", argc, argv)) > 0) Threads = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-binary", argc, argv)) > 0) binaryFlag = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-note", argc, argv)) > 0) note = argv[i + 1];

}

int main(int argc, char **argv) {
    setparameters(argc, argv);
    init();
    prepare();
    test(NULL);
    return 0;
}
