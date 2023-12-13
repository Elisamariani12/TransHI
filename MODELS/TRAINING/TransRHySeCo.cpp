#define REAL float   
#define INT int

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <list>
#include <random>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <pthread.h>
#include <sys/mman.h>

using namespace std;

const REAL pi = 3.141592653589793238462643383;

INT number_of_iteration;
float percentage_of_negatives_generated;
bool use_structure;
bool use_ontology;


INT typeOf_ID=275;
INT threads = 3;
INT bernFlag = 1;
INT loadBinaryFlag = 0;
INT outBinaryFlag = 0;
INT trainTimes = 500;
INT nbatches = 50;
INT dimension = 100;
INT dimensionR = 100;
REAL alpha = 0.001;
REAL margin = 1;
INT count_my_negatives=0;
float SlovinError=0.05f;
INT SlovinTries;

string inPath = "";
string outPath = "";
string loadPath = "";
string initPath = "";
string note = "";
string old_note="";


INT *lefHead, *rigHead;
INT *lefTail, *rigTail;

struct Triple {
	INT h, r, t;
};

//3D matrix to store all the negatives create (to not repeat them) and the training data (to not use them as negatives)
typedef std::unordered_map<int, bool> InnerMap;
typedef std::unordered_map<int, InnerMap> MiddleMap;
typedef std::unordered_map<int, MiddleMap> OuterMap;
class Sparse3DMatrix {
public:
    void set(int x, int y, int z) {
            data[x][y][z] = true;
    }

    
    bool get(int x, int y, int z) {
        auto x_it = data.find(x);
        if (x_it != data.end()) {
            auto y_it = x_it->second.find(y);
            if (y_it != x_it->second.end()) {
                return y_it->second.count(z) > 0;
            }
        }
        return false;
    }

private:
    OuterMap data;
};
Sparse3DMatrix adjacencyList_all_trainTriples; 	

struct cmp_head {
	bool operator()(const Triple &a, const Triple &b) {
		return (a.h < b.h)||(a.h == b.h && a.r < b.r)||(a.h == b.h && a.r == b.r && a.t < b.t);
	}
};

struct cmp_tail {
	bool operator()(const Triple &a, const Triple &b) {
		return (a.t < b.t)||(a.t == b.t && a.r < b.r)||(a.t == b.t && a.r == b.r && a.h < b.h);
	}
};

struct cmp_list {
	INT minimal(INT a,INT b) {
		if (a < b) return b;
		return a;
	}
	bool operator()(const Triple &a, const Triple &b) {
		return (minimal(a.h, a.t) < minimal(b.h, b.t));
	}
};

Triple *trainHead, *trainTail, *trainList;
//to save negatives coming from a previous iteration
std::map<int, std::vector<Triple>> prevIter_negativesList;

//NEEDED TO HANDLE THE STRUCTURE-BASED NEGATIVES GENERATION
std::vector<vector<int>> mapNeighbors;
std::vector<float> mapCCs;
vector<int> testineT_neigh;
vector<int> testineH_neigh;

//NEEDED TO HANDLE THE ONTOLOGY-BASED NEGATIVES GENERATION
std::map<int, std::vector<int>> mapDisjointClasses;
std::map<int, std::vector<int>> mapDisjointClasses_for_head_change;
std::map<int, std::vector<int>> mapDomains;
std::map<int, std::vector<int>> mapNotAdmitted_domain;
std::map<int, std::vector<int>> mapRanges;
std::map<int, std::vector<int>> mapNotAdmitted_range;
std::map<int, std::vector<int>> mapClassesToEntities;
std::map<int, std::vector<int>> mapEntitiesToClasses;
std::map<int, std::vector<int>> mapSuperClasses;
std::map<int, std::vector<int>> mapUsedClasses_range;
std::map<int, std::vector<int>> mapUsedClasses_domain;
std::map<int, std::vector<int>> mapUsedClasses_disj_h;
vector<bool> asymmetric_property_to_be_used;
vector<bool> irreflexive_proprerty_to_be_used;
vector<int> testineT_ontology;
vector<int> testineH_ontology;

// % of entities that are instances of no class
INT Perc_EntitiesWithNoClass;
INT Perc_EntitiesWithNoClass_nr;

//numbers of relations,entity, train triples, train triples from previous iteration
INT relationTotal, entityTotal, tripleTotal, previous_iteration_negatives_Total;
INT *freqRel, *freqEnt;
REAL *left_mean, *right_mean;
REAL *relationVec, *entityVec, *matrix;
REAL *relationVecDao, *entityVecDao, *matrixDao;
REAL *tmpValue;

pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;


/*
	There are some math functions for the program initialization.
*/

unsigned long long *next_random;

unsigned long long randd(INT id) {
	next_random[id] = next_random[id] * (unsigned long long)25214903917 + 11;
	return next_random[id];
}

INT rand_max(INT id, INT x) {
	INT res = randd(id) % x;
	while (res<0)
		res+=x;
	return res;
}

REAL rand(REAL min, REAL max) {
	return min + (max - min) * rand() / (RAND_MAX + 1.0);
}

REAL normal(REAL x, REAL miu,REAL sigma) {
	return 1.0/sqrt(2*pi)/sigma*exp(-1*(x-miu)*(x-miu)/(2*sigma*sigma));
}

REAL randn(REAL miu,REAL sigma, REAL min ,REAL max) {
	REAL x, y, dScope;
	do {
		x = rand(min,max);
		y = normal(x,miu,sigma);
		dScope=rand(0.0,normal(miu,miu,sigma));
	} while (dScope > y);
	return x;
}

void norm(REAL *con, INT dimension) {
	REAL x = 0;
	for (INT  ii = 0; ii < dimension; ii++)
		x += (*(con + ii)) * (*(con + ii));
	x = sqrt(x);
	if (x>1)
		for (INT ii=0; ii < dimension; ii++)
			*(con + ii) /= x;
}

void norm(REAL *con, REAL *matrix) {
	REAL tmp, x;
	INT last;
	x = 0;
	last = 0;
	for (INT ii = 0; ii < dimensionR; ii++) {
		tmp = 0;
		for (INT jj=0; jj < dimension; jj++) {
			tmp += matrix[last] * con[jj];
			last++;
		}
		x += tmp * tmp;
	}
	if (x>1) {
		REAL lambda = 1;
		for (INT ii = 0, last = 0; ii < dimensionR; ii++, last += dimension) {
			tmp = 0;
			for (INT jj = 0; jj < dimension; jj++)
				tmp += ((matrix[last + jj] * con[jj]) * 2);
			for (INT jj = 0; jj < dimension; jj++) {
				matrix[last + jj] -= alpha * lambda * tmp * con[jj];
				con[jj] -= alpha * lambda * tmp * matrix[last + jj];
			}
		}
	}
}

void norm(INT h, INT t, INT r, INT j) {
		norm(relationVecDao + dimensionR * r, dimensionR);
		norm(entityVecDao + dimension * h, dimension);
		norm(entityVecDao + dimension * t, dimension);
		norm(entityVecDao + dimension * j, dimension);
		norm(entityVecDao + dimension * h, matrixDao + dimension * dimensionR * r);
		norm(entityVecDao + dimension * t, matrixDao + dimension * dimensionR * r);
		norm(entityVecDao + dimension * j, matrixDao + dimension * dimensionR * r);
}

/*
	function to add negatives/training triples to the 3d matrix
*/
void addEdge(int from, int relation, int to) {
		adjacencyList_all_trainTriples.set(from,to,relation);
}
/*
	function to order the neighbors based on cc
*/
bool compareByMapValue_cc(const int& a, const int& b, const std::vector<float>& mymap) {
    return mymap[a] > mymap[b];		//DECREASING order
}
/*
	retrieve the embedding for the entity 'entity'
*/
std::vector<float> getEmbeddingforEntity(int entity){
	vector<float> result(dimension);
	for(int i=0;i<dimension;i++){
		result[i]=entityVec[entity*dimension+i];
	}

	return result;
}
/*
	L2 norm of the vector v
*/
float normEmbedding(const std::vector<float>& v) {
    float result = 0.0f;
    for (size_t i = 0; i < v.size(); i++) {
        result += v[i] * v[i];
    }
    return std::sqrt(result);
}
/*
	compute the cosine similarity
*/
float computeSimilarity(vector<float> embedding1,vector<float> embedding2){
	float result=0;
	for(int i=0;i<dimension;i++){
		result+=embedding1[i]*embedding2[i];
	}
	
	return result;
}
/*
	scanning the train triples, fill two maps: one that link every class to its entities and one that maps every entity to its classes
*/
void fillMapsOfClassesAndEntities(){
	for(int i=0;i<tripleTotal;i++){
		if(trainList[i].r==typeOf_ID){
			mapClassesToEntities[trainList[i].t].push_back(trainList[i].h);
			mapEntitiesToClasses[trainList[i].h].push_back(trainList[i].t);
		}
	}
}
/*
	compute the percentage of entities that have no class: entities that don't appear in the map mapEntitiesToClasses
*/
void percentageEntitiesWithNoClass(){
	int count_noclass_ent=0;
	for(int i=0;i<entityTotal;i++){
		if(mapEntitiesToClasses.find(i)==mapEntitiesToClasses.end()){
			count_noclass_ent++;
		}
	}
	
	//rounded percentage
	Perc_EntitiesWithNoClass=(double)10*count_noclass_ent/entityTotal;
	Perc_EntitiesWithNoClass=(double)10*Perc_EntitiesWithNoClass;

	//not rounded percentage
	Perc_EntitiesWithNoClass_nr=(double)100*count_noclass_ent/entityTotal;

	cout<<"The entities with no class are:"<<Perc_EntitiesWithNoClass<<" % \n";

}
/*
	order the neighbors based on their cc
*/
void order_neighbors(){
	cout<<"Sorting the neighbors : \n";
	for(int entity_id=0;entity_id<mapNeighbors.size();entity_id++){
		std::sort(mapNeighbors[entity_id].begin(), mapNeighbors[entity_id].end(), [&](const int& a, const int& b) {return compareByMapValue_cc(a, b, mapCCs);});
	}
	cout<<"done \n";
}
/*
	add superclasses in the domain because of the entailed axiom: p-domain->x,x-subclass->y implies p-domain->y
*/
void AddSuperClassesForGenericity_domain(){
	for(const auto& myentry:mapDomains){
        vector<int> my_super_classes;
		for(const auto& mydomain_class:myentry.second){
			if(mapSuperClasses.find(mydomain_class)!=mapSuperClasses.end()){
                my_super_classes.insert(my_super_classes.end(),mapSuperClasses[mydomain_class].begin(),mapSuperClasses[mydomain_class].end());
			}
		}
        for(const auto& superclass:my_super_classes){
            if(find(myentry.second.begin(),myentry.second.end(),superclass)==myentry.second.end()){
                mapDomains[myentry.first].push_back(superclass);
            }
        }			
	}
}
/*
	add superclasses in the range because of the entailed axiom: p-range->x,x-subclass->y implies p-range->y
*/
void AddSuperClassesForGenericity_range(){
	for(const auto& myentry:mapRanges){
        vector<int> my_super_classes;
		for(const auto& myrange_class:myentry.second){
			if(mapSuperClasses.find(myrange_class)!=mapSuperClasses.end()){
                my_super_classes.insert(my_super_classes.end(),mapSuperClasses[myrange_class].begin(),mapSuperClasses[myrange_class].end());
			}
		}
        for(const auto& superclass:my_super_classes){
            if(find(myentry.second.begin(),myentry.second.end(),superclass)==myentry.second.end()){
                mapRanges[myentry.first].push_back(superclass);
            }
        }			
	}
}
/*
	create map of classes that are not admitted as the domain of each property
*/
void CreateMapNotAdmitted_domain(){
	for(const auto& entry:mapDomains){
		vector<int> my_domain_classes=entry.second;
		for (const auto& pair : mapClassesToEntities) {
        	if(find(my_domain_classes.begin(),my_domain_classes.end(),pair.first)==my_domain_classes.end()){
				mapNotAdmitted_domain[entry.first].push_back(pair.first);
			}
   		}
	}
	cout<<"Le classi non ammesse per la relazione 281 sono:"<<mapNotAdmitted_domain[281].size();
}
/*
	create map of classes that are not admitted as the range of each property
*/
void CreateMapNotAdmitted_range(){
	for(const auto& entry:mapRanges){
		vector<int> my_range_classes=entry.second;
		for (const auto& pair : mapClassesToEntities) {
        	if(find(my_range_classes.begin(),my_range_classes.end(),pair.first)==my_range_classes.end()){
				mapNotAdmitted_range[entry.first].push_back(pair.first);
			}
   		}
	}
	cout<<"Le classi non ammesse per la relazione 281 sono:"<<mapNotAdmitted_range[281].size();
}



void load() {
    FILE *fin;
    INT tmp;
    fin = fopen((loadPath + "entity2vec" + note + ".vec").c_str(), "r");
    for (INT i = 0; i < entityTotal; i++) {
        INT last = i * dimension;
        for (INT j = 0; j < dimension; j++)
            tmp = fscanf(fin, "%f", &entityVec[last + j]);
    }
    fclose(fin);
    fin = fopen((loadPath + "relation2vec" + note + ".vec").c_str(), "r");
    for (INT i = 0; i < relationTotal; i++) {
        INT last = i * dimensionR;
        for (INT j = 0; j < dimensionR; j++)
            tmp = fscanf(fin, "%f", &relationVec[last + j]);
    }
    fclose(fin);

    fin = fopen((loadPath + "A" + note + ".vec").c_str(), "r");
    for (INT i = 0; i < relationTotal; i++)
            for (INT jj = 0; jj < dimension; jj++)
                for (INT ii = 0; ii < dimensionR; ii++)
                    tmp = fscanf(fin, "%f", &matrix[i * dimensionR * dimension + jj + ii * dimension]);
    fclose(fin);
}


/*
	Read triples from the training file.
*/

void init() {

	FILE *fin;
	INT tmp;

	// Retrieve the ID related to the 'type' relationship
	char buffer[1024]; 
	fin = fopen((inPath+"relation2id.txt").c_str(), "r");
	fscanf(fin, "%d", &relationTotal);
	for (int i = 0; i < relationTotal; ++i) {
        fscanf(fin, "%s", buffer);
		fscanf(fin, "%d", &tmp);
        std::string relation=buffer;
        if (relation == "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>") {
            typeOf_ID = tmp;
            break;
        }
    }
	cout<<"The ID of typeOf rel. is:"<<typeOf_ID<<"\n";
	fclose(fin);

	fin = fopen((inPath + "entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &entityTotal);
	fclose(fin);

	relationVec = (REAL *)calloc(relationTotal * dimensionR * 2 + entityTotal * dimension * 2 + relationTotal * dimension * dimensionR * 2, sizeof(REAL));
	relationVecDao = relationVec + relationTotal * dimensionR;
	entityVec = relationVecDao + relationTotal * dimensionR;
	entityVecDao = entityVec + entityTotal * dimension;
	matrix = entityVecDao + entityTotal * dimension;
	matrixDao = matrix + dimension * dimensionR * relationTotal;
	
	freqRel = (INT *)calloc(relationTotal + entityTotal, sizeof(INT));
	freqEnt = freqRel + relationTotal;

	for (INT i = 0; i < relationTotal; i++) {
		for (INT ii=0; ii < dimensionR; ii++)
			relationVec[i * dimensionR + ii] = randn(0, 1.0 / dimensionR, -6 / sqrt(dimensionR), 6 / sqrt(dimensionR));
	}
	for (INT i = 0; i < entityTotal; i++) {
		for (INT ii=0; ii < dimension; ii++)
			entityVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
		norm(entityVec + i * dimension, dimension);
	}

	for (INT i = 0; i < relationTotal; i++)
		for (INT j = 0; j < dimensionR; j++)
			for (INT k = 0; k < dimension; k++)
				matrix[i * dimension * dimensionR + j * dimension + k] =  randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));


 	// set a limit of tries to do to look for the ontology-based triples to speed up the computation
	SlovinTries=entityTotal/((double)1+SlovinError*SlovinError*entityTotal);

	fin = fopen((inPath + "train2id_Consistent_withAugmentation.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &tripleTotal);
	trainHead = (Triple *)calloc(tripleTotal * 3, sizeof(Triple));
	trainTail = trainHead + tripleTotal;
	trainList = trainTail + tripleTotal;
	for (INT i = 0; i < tripleTotal; i++) {
		tmp = fscanf(fin, "%d", &trainList[i].h);
		tmp = fscanf(fin, "%d", &trainList[i].t);
		tmp = fscanf(fin, "%d", &trainList[i].r);
		freqEnt[trainList[i].t]++;
		freqEnt[trainList[i].h]++;
		freqRel[trainList[i].r]++;
		trainHead[i] = trainList[i];
		trainTail[i] = trainList[i];
	}
	fclose(fin);

	/*----------------------- IF IT'S NOT THE FIRST ITERATION, LOAD THE NEGATIVES FROM PREVIOUS ITERATIONS --------------------------*/
	if(number_of_iteration>1){
		fin = fopen((inPath +"inconsistent_wrongly_classified.txt").c_str(), "r");
		tmp = fscanf(fin, "%d", &previous_iteration_negatives_Total);
		
		int id_curr=-1;
		for (INT i = 0; i < previous_iteration_negatives_Total; i++) {
				tmp = fscanf(fin, "%d", &id_curr);
				Triple new_neg;
				tmp = fscanf(fin, "%d", &new_neg.h);
				tmp = fscanf(fin, "%d", &new_neg.t);
				tmp = fscanf(fin, "%d", &new_neg.r);
				prevIter_negativesList[id_curr].push_back(new_neg);
		}
		fclose(fin);
	}

/*----------------------- ALLOCATE THE 3d MATRIX to store used negatives and training triples AND POPULATE IT with training triples--------------------------------------*/
	cout<<"populating adjacency list : ";
	int i;
	for(i=0;i<tripleTotal;i++){
		//cout<<trainList[i].h<<" "<<trainList[i].t<<" "<<trainList[i].r<<"\n";
		addEdge(trainList[i].h,trainList[i].r,trainList[i].t);
	}	
	cout<<"done \n";

	/*----------------------------------INITIALIZE STRUCTURES NEEDED FOR STANDARD RANDOM CREATION - same as TransE----------------------------------------------*/

	sort(trainHead, trainHead + tripleTotal, cmp_head());
	sort(trainTail, trainTail + tripleTotal, cmp_tail());
	sort(trainList, trainList + tripleTotal, cmp_list());

	lefHead = (INT *)calloc(entityTotal * 4, sizeof(INT));
	rigHead = lefHead + entityTotal;
	lefTail = rigHead + entityTotal;
	rigTail = lefTail + entityTotal;

	memset(rigHead, -1, sizeof(INT)*entityTotal);
	memset(rigTail, -1, sizeof(INT)*entityTotal);

	for (INT i = 1; i < tripleTotal; i++) {
		if (trainTail[i].t != trainTail[i - 1].t) {
			rigTail[trainTail[i - 1].t] = i - 1;
			lefTail[trainTail[i].t] = i;
		}
		if (trainHead[i].h != trainHead[i - 1].h) {
			rigHead[trainHead[i - 1].h] = i - 1;
			lefHead[trainHead[i].h] = i;
		}
	}

	rigHead[trainHead[tripleTotal - 1].h] = tripleTotal - 1;
	rigTail[trainTail[tripleTotal - 1].t] = tripleTotal - 1;


	left_mean = (REAL *)calloc(relationTotal * 2,sizeof(REAL));
	right_mean = left_mean + relationTotal;

	for (INT i = 0; i < entityTotal; i++) {
		for (INT j = lefHead[i] + 1; j <= rigHead[i]; j++)
			if (trainHead[j].r != trainHead[j - 1].r)
				left_mean[trainHead[j].r] += 1.0;
		if (lefHead[i] <= rigHead[i])
			left_mean[trainHead[lefHead[i]].r] += 1.0;
		for (INT j = lefTail[i] + 1; j <= rigTail[i]; j++)
			if (trainTail[j].r != trainTail[j - 1].r)
				right_mean[trainTail[j].r] += 1.0;
		if (lefTail[i] <= rigTail[i])
			right_mean[trainTail[lefTail[i]].r] += 1.0;
	}

	for (INT i = 0; i < relationTotal; i++) {
		left_mean[i] = freqRel[i] / left_mean[i];
		right_mean[i] = freqRel[i] / right_mean[i];
	}


	//---------------------------Initialize embeddings with transE results --------------
	//if(initPath!=""){	
		cout<<"Initialize embeddings with TransE_30\n";
		for (INT i = 0; i < relationTotal; i++)
			for (INT j = 0; j < dimensionR; j++)
				for (INT k = 0; k < dimension; k++)
					if (j == k)
						matrix[i * dimension * dimensionR + j * dimension + k] = 1;
					else
						matrix[i * dimension * dimensionR + j * dimension + k] = 0;
		FILE* f1 = fopen((inPath + "entity2vec_init.vec").c_str(),"r");
		for (INT i = 0; i < entityTotal; i++) {
			for (INT ii = 0; ii < dimension; ii++)
				tmp = fscanf(f1, "%f", &entityVec[i * dimension + ii]);
			norm(entityVec + i * dimension, dimension);
		}
		fclose(f1);
		FILE* f2 = fopen((inPath + "relation2vec_init.vec").c_str(),"r");
		for (INT i=0; i < relationTotal; i++) {
			for (INT ii=0; ii < dimension; ii++)
				tmp = fscanf(f2, "%f", &relationVec[i * dimensionR + ii]);
		}
		fclose(f2);
	//}

	printf("Inizializzato con TransE\n");

/*----------------ADDITIONAL PART FOR LOADING THE INPUT FILES WITH THE ONTOLOGY AXIOMS NEEDED TO BUILD THE NEGATIVES --------------------------------*/
	
	// load disjoint classes axioms
	if(percentage_of_negatives_generated>0){
	cout<<"loading Disjoint-classes file: ";
	std::ifstream filedisj((inPath +"DisjointWith_axioms.txt").c_str());
    if (filedisj.is_open()) {
        std::string line;
        while (std::getline(filedisj, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
          
          if (iss.peek() == ',')
                  iss.ignore();

          std::vector<int> values;
          int value;
          while (iss >> value) {
              mapDisjointClasses[key].push_back(value);
              if (iss.peek() == ',')
                  iss.ignore();
          }
        }
        filedisj.close();
	} else {
	std::cerr << "Failed to open file: DisjointWith_axioms.txt" << std::endl;
	}
    mapDisjointClasses_for_head_change=mapDisjointClasses;
	cout<<"done \n";

	// load domain axioms
	cout<<"loading Domain-axioms file: ";
	std::ifstream filedom((inPath +"Domain_axioms.txt").c_str());
    if (filedom.is_open()) {
        std::string line;
        while (std::getline(filedom, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
          
          if (iss.peek() == ',')
                  iss.ignore();

          std::vector<int> values;
          int value;
          while (iss >> value) {
              mapDomains[key].push_back(value);
              if (iss.peek() == ',')
                  iss.ignore();
          }
        }
        filedom.close();
	} else {
	std::cerr << "Failed to open file: Domain_axioms.txt" << std::endl;
	}
	cout<<"done \n";

	// load range axioms
	cout<<"loading Range-axioms file: ";
	std::ifstream filerange((inPath +"Range_axioms.txt").c_str());
    if (filerange.is_open()) {
        std::string line;
        while (std::getline(filerange, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
          
          if (iss.peek() == ',')
                  iss.ignore();

          std::vector<int> values;
          int value;
          while (iss >> value) {
              mapRanges[key].push_back(value);
              if (iss.peek() == ',')
                  iss.ignore();
          }
        }
        filerange.close();
	} else {
	std::cerr << "Failed to open file: Range_axioms.txt" << std::endl;
	}
	cout<<"done \n";

	// load superclasses axioms
	cout<<"loading SuperClasses file: ";
	std::ifstream fileSuperClasses((inPath +"SuperClasses_axioms.txt").c_str());
    if (fileSuperClasses.is_open()) {
        std::string line;
        while (std::getline(fileSuperClasses, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
          
          if (iss.peek() == ',')
                  iss.ignore();

          std::vector<int> values;
          int value;
          while (iss >> value) {
              mapSuperClasses[key].push_back(value);
              if (iss.peek() == ',')
                  iss.ignore();
          }
        }
        fileSuperClasses.close();
	} else {
	std::cerr << "Failed to open file: SuperClasses_axioms.txt" << std::endl;
	}
	cout<<"done \n";

	// load irreflexive relations axioms
	cout<<"loading irreflexive relations file: ";
	irreflexive_proprerty_to_be_used.resize(tripleTotal,false);
	std::ifstream fileIrreflexiveProp((inPath +"IrreflexiveProperties_axioms.txt").c_str());
    if (fileIrreflexiveProp.is_open()) {
        std::string line;
        while (std::getline(fileIrreflexiveProp, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
		  irreflexive_proprerty_to_be_used[key]=true;
        }
        fileIrreflexiveProp.close();
	} else {
	std::cerr << "Failed to open file: IrreflexiveProperties_axioms.txt" << std::endl;
	}
	cout<<"done \n";

	// load asymmetric relations axioms
	cout<<"loading asymmetric relations file: ";
	asymmetric_property_to_be_used.resize(tripleTotal,false);
	std::ifstream fileAsymmProp((inPath +"AsymmetricProperties_axioms.txt").c_str());
    if (fileAsymmProp.is_open()) {
        std::string line;
        while (std::getline(fileAsymmProp, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
		  asymmetric_property_to_be_used[key]=true;
        }
        fileAsymmProp.close();
	} else {
	std::cerr << "Failed to open file: AsymmetricProperties_axioms.txt" << std::endl;
	}
	cout<<"done \n";


    //Create 2 maps: 1 map of all the classes and the respective entities & 1 viceversa
	fillMapsOfClassesAndEntities();

	//add superclasses in domain and range for entailed axioms
	AddSuperClassesForGenericity_domain();
	AddSuperClassesForGenericity_range();

	//create a map with not admitted classes domain & range
    CreateMapNotAdmitted_domain();
	CreateMapNotAdmitted_range();

	//compute percentage entities witout class
	percentageEntitiesWithNoClass();
    }

	/*----------------------------------------------------------------------------------------------------------------------*/	

	/*--ADDITIONAL PART TO ALLOCATE SPACE FOR SUPPORT STRUCTURES NEEDED TO AVOID REPEATING CHECKS OVER NEGATIVE TRIPLES------*/
	cout<<"allocating support structures to save neighbors indexes:";
	testineH_neigh.resize(tripleTotal,0);
	testineT_neigh.resize(tripleTotal,0);
	testineH_ontology.resize(tripleTotal,0);
	testineT_ontology.resize(tripleTotal,0);
	cout<<"done \n";

	/*------------------------------------------------------------------------------------------------------------------------------*/

	/*ADDITIONAL PART FOR LOADING THE INPUT FILE WITH THE NEIGHBORS --------------------------------------------------------*/
    cout<<"loading neighbors file: ";

	std::ifstream file((inPath +"file_Neighbors.txt").c_str());
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
          std::istringstream iss(line);
          int key;
          iss >> key;
          
          if (iss.peek() == ',')
                  iss.ignore();

          std::vector<int> values;
          int value;
          while (iss >> value) {
              values.push_back(value);
              if (iss.peek() == ',')
                  iss.ignore();
          }
		  if(key>=mapNeighbors.size()){
			mapNeighbors.resize(key+1);
		  }
          mapNeighbors[key] = values;
        }
        file.close();
    } else {
        std::cerr << "Failed to open file: file_Neighbors.txt" << std::endl;
    }
	    cout<<"done \n";

	/*------------ LOAD CCs --------------------------------------------------------------*/
		std::ifstream file2((inPath +"CCs.txt").c_str());
		if (file2.is_open()) {
			std::string line;
			while (std::getline(file2, line)) {
				std::istringstream iss(line);
				int key;
				float value;
				if (iss >> key >> value) {
					if(key>=mapCCs.size())
						mapCCs.resize(key+1);
					mapCCs[key]=value;
				}
			}
			file2.close();
		} else {
			std::cerr << "Failed to open file: /content/CCs.txt" << std::endl;
		}			

	/*-----------------------ORDER AND CUT THE NEIGHBORS MAP BASED ON CC -------------------------*/
	if(use_structure==true){
	order_neighbors();
	}

	/*-------------------------------SECOND+ ITERATION: LOAD PREVIOUS EMBEDDINGS  ----------------*/
	if(number_of_iteration>1){
		load();
	}

  cout << "End init \n";

}


/*
	Training process of transR.
*/

INT transRLen;
INT transRBatch;
REAL res;

REAL calc_sum(INT e1, INT e2, INT rel, REAL *tmp1, REAL *tmp2) {
	INT lastM = rel * dimension * dimensionR;
	INT last1 = e1 * dimension;
	INT last2 = e2 * dimension;
	INT lastr = rel * dimensionR;
	REAL sum = 0;
	for (INT ii = 0; ii < dimensionR; ii++) {
		tmp1[ii] = tmp2[ii] = 0;
		for (INT jj = 0; jj < dimension; jj++) {
			tmp1[ii] += matrix[lastM + jj] * entityVec[last1 + jj];
			tmp2[ii] += matrix[lastM + jj] * entityVec[last2 + jj];
		}
		lastM += dimension;
		sum += fabs(tmp1[ii] + relationVec[lastr + ii] - tmp2[ii]);
	}
	return sum;
}

void gradient(INT e1_a, INT e2_a, INT rel_a, INT belta, INT same, REAL *tmp1, REAL *tmp2) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimensionR;
	INT lastM = rel_a * dimensionR * dimension;
	REAL x;
	for (INT ii=0; ii < dimensionR; ii++) {
		x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii];
		if (x > 0)
			x = belta * alpha;
		else
			x = -belta * alpha;
		for (INT jj = 0; jj < dimension; jj++) {
			matrixDao[lastM + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
			entityVecDao[lasta1 + jj] -= x * matrix[lastM + jj];
			entityVecDao[lasta2 + jj] += x * matrix[lastM + jj];
		}
		relationVecDao[lastar + ii] -= same * x;
		lastM = lastM + dimension;
	}
}

void train_kb(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, REAL *tmp) {
	REAL sum1 = calc_sum(e1_a, e2_a, rel_a, tmp, tmp + dimensionR);
	REAL sum2 = calc_sum(e1_b, e2_b, rel_b, tmp + dimensionR * 2, tmp + dimensionR * 3);
	if (sum1 + margin > sum2) {
		res += margin + sum1 - sum2;
		gradient(e1_a, e2_a, rel_a, -1, 1, tmp, tmp + dimensionR);
    	gradient(e1_b, e2_b, rel_b, 1, 1, tmp + dimensionR * 2, tmp + dimensionR * 3);
	}
}

INT corrupt_head(INT id, INT h, INT r) {
	INT lef, rig, mid, ll, rr;
	lef = lefHead[h] - 1;
	rig = rigHead[h];
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].r >= r) rig = mid; else
		lef = mid;
	}
	ll = rig;
	lef = lefHead[h];
	rig = rigHead[h] + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].r <= r) lef = mid; else
		rig = mid;
	}
	rr = lef;
	INT tmp = rand_max(id, entityTotal - (rr - ll + 1));
	if (tmp < trainHead[ll].t) return tmp;
	if (tmp > trainHead[rr].t - rr + ll - 1) return tmp + rr - ll + 1;
	lef = ll, rig = rr + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].t - mid + ll - 1 < tmp)
			lef = mid;
		else 
			rig = mid;
	}
	return tmp + lef - ll + 1;
}

INT corrupt_tail(INT id, INT t, INT r) {
	INT lef, rig, mid, ll, rr;
	lef = lefTail[t] - 1;
	rig = rigTail[t];
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].r >= r) rig = mid; else
		lef = mid;
	}
	ll = rig;
	lef = lefTail[t];
	rig = rigTail[t] + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].r <= r) lef = mid; else
		rig = mid;
	}
	rr = lef;
	INT tmp = rand_max(id, entityTotal - (rr - ll + 1));
	if (tmp < trainTail[ll].h) return tmp;
	if (tmp > trainTail[rr].h - rr + ll - 1) return tmp + rr - ll + 1;
	lef = ll, rig = rr + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].h - mid + ll - 1 < tmp)
			lef = mid;
		else 
			rig = mid;
	}
	return tmp + lef - ll + 1;
}

//---------------------------PROTOYPE OF THE FUNCTIONS FOR CREATION OF NEGATIVES ----------------------------

Triple findReplacement(int h, int r, int t, int postriple_index);
std::vector<int> getNeighborsToConsider(std::vector<int> my_neighbors, std::map<int, float> mapCC_or_degree);
Triple One_negative_creation_with_ontology(int index_positive);
Triple find_Negative_from_previous_iteration(int index_positive);
int find_not_similar(int entity);

//----------------------------------------------------------------------------------------------------------


void* trainMode(void *con) {
	INT id, i, j, pr;
	id = (unsigned long long)(con);
	next_random[id] = rand();
	REAL *tmp = tmpValue + id * dimensionR * 4;

	/*Batch is the number of triples in a batch*/
	for (INT k = transRBatch / threads; k >= 0; k--) {
i = rand_max(id, transRLen);   		
      
    	double rand_num=(double)100*rand()/RAND_MAX;

		// NEGATIVES CREATION WITH ONTOLOGY
		//i check the percentage of ontology-based negatives and try  to create a negative with the ontology
		// if it's not a positive triple for which i can use the ontology (or based on the percentage of entities without class) i'm going to try with structure
		if((rand_num<percentage_of_negatives_generated && (use_ontology||use_structure))||(number_of_iteration>1)){
			
			//if iteration=1 i use the ontology based in amount depending on the percentage of entities without class
			//if iteration=2 i use the ontology always: because in that case the ontology triples are the ones created in the previous iteration
			double rand_num2=(double)100*rand()/RAND_MAX;
			if((use_ontology==true && number_of_iteration==1 && rand_num2>Perc_EntitiesWithNoClass)||(number_of_iteration>1 && rand_num2>Perc_EntitiesWithNoClass_nr && Perc_EntitiesWithNoClass_nr>=50)|| (number_of_iteration>1 && Perc_EntitiesWithNoClass_nr<50)){

                bool found=false;
				Triple mynegative;mynegative.h=-1;mynegative.r=-1;mynegative.t=-1;
				if(number_of_iteration>1){	// try to see if i still have some negatives from previous iterations for this triple to use
					mynegative=find_Negative_from_previous_iteration(i);
					if(mynegative.h!=-1)found=true;
				}
				else{						//create a negative exploiting ontological axioms
					mynegative=One_negative_creation_with_ontology(i);
				}
				if(mynegative.h!=-1){
					//compute the loss, update embeddings
					train_kb(trainList[i].h, trainList[i].t, trainList[i].r, mynegative.h, mynegative.t, mynegative.r,tmp);
					//normalize the modified embedddings
					if(mynegative.h==trainList[i].h){
						norm(trainList[i].h, trainList[i].t, trainList[i].r, mynegative.t);
					}
					else if (mynegative.t==trainList[i].t) {
						norm(trainList[i].h, trainList[i].t, trainList[i].r, mynegative.h);
					} 
					count_my_negatives++;
					continue;
				}
			}
			// NEGATIVES CREATION WITH STRUCTURE
			// I'm going to use structure if the ontology based triple could not be generated (or because of the percentage of entities without class)
			if(number_of_iteration==1 && use_structure==true ){
				//create negative with structure
				Triple negative = findReplacement(trainList[i].h, trainList[i].r, trainList[i].t,i);
				if(negative.h!=-1){ 
				//compute the loss, update embeddings
				train_kb(trainList[i].h, trainList[i].t, trainList[i].r, negative.h, negative.t, negative.r,tmp);
				//normalize the modified embedddings
				if(negative.h==trainList[i].h){
					norm(trainList[i].h, trainList[i].t, trainList[i].r, negative.t);
				}
				else if (negative.t==trainList[i].t) {
					norm(trainList[i].h, trainList[i].t, trainList[i].r, negative.h);
				} 
				
				count_my_negatives++;
				continue;
				}
			}

    	}
		// STANDARD RANDOM CREATION OF THE NEGATIVE - as in TransE
		
		if (bernFlag)
			pr = 1000 * right_mean[trainList[i].r] / (right_mean[trainList[i].r] + left_mean[trainList[i].r]);
		else
			pr = 500;

		if (randd(id) % 1000 < pr) {
			//if iter=1 no constraints on  random triples
			if(number_of_iteration==1){
				j = corrupt_head(id, trainList[i].h, trainList[i].r);
			}
			else {
				//if iter>1 apply constraints on random
				//if entities with no class>50% i don't use always the similarity constraint (because i have to differentiate more the entities used)
				if(Perc_EntitiesWithNoClass_nr>=50){
					do{
						if((double)rand()/RAND_MAX<0.5) j=find_not_similar(trainList[i].t);
						else j=rand_max(0,entityTotal-1);
					}while(adjacencyList_all_trainTriples.get(trainList[i].h,j,trainList[i].r)==true || abs(calc_sum(trainList[i].h,j,trainList[i].r,tmp,tmp+dimensionR))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR)));
				}else{
					do{
						j=find_not_similar(trainList[i].t);
					}while(adjacencyList_all_trainTriples.get(trainList[i].h,j,trainList[i].r)==true || abs(calc_sum(trainList[i].h,j,trainList[i].r,tmp,tmp+dimensionR))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR)));
				}
			}
			//compute the loss, update embeddings
			train_kb(trainList[i].h, trainList[i].t, trainList[i].r, trainList[i].h, j, trainList[i].r,tmp);
			//normalize the modified embedddings
			norm(trainList[i].h, trainList[i].t, trainList[i].r, j);
		} 
		else {
			//if iter=1 no constraints on  random triples
			if(number_of_iteration==1){
				j = corrupt_tail(id, trainList[i].t, trainList[i].r);
			}
			else {
				//if iter>1 apply constraints on random
				//if entities with no class>50% i don't use always the similarity constraint (because i have to differentiate more the entities used)
				if(Perc_EntitiesWithNoClass_nr>=50){
					do{
						if((double)rand()/RAND_MAX<0.5) j=find_not_similar(trainList[i].h);
						else j=rand_max(0,entityTotal-1);
					}while(adjacencyList_all_trainTriples.get(j,trainList[i].t,trainList[i].r)==true || abs(calc_sum(j,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR)));
				}else{
					do{
						j=find_not_similar(trainList[i].h);
					}while(adjacencyList_all_trainTriples.get(j,trainList[i].t,trainList[i].r)==true || abs(calc_sum(j,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r,tmp,tmp+dimensionR)));
				}
			}
			//compute the loss, update embeddings
			train_kb(trainList[i].h, trainList[i].t, trainList[i].r, j, trainList[i].t, trainList[i].r,tmp);
			//normalize the modified embedddings
			norm(trainList[i].h, trainList[i].t, trainList[i].r, j);
		}

	}
		pthread_exit(NULL);

}



void train(void *con) {
	transRLen = tripleTotal;
	transRBatch = transRLen / nbatches;
	next_random = (unsigned long long *)calloc(threads, sizeof(unsigned long long));
	tmpValue = (REAL *)calloc(threads * dimensionR * 4, sizeof(REAL));
	memcpy(relationVecDao, relationVec, dimensionR * relationTotal * sizeof(REAL));
	memcpy(entityVecDao, entityVec, dimension * entityTotal * sizeof(REAL));
	memcpy(matrixDao, matrix, dimension * relationTotal * dimensionR * sizeof(REAL));
	for (INT epoch = 0; epoch < trainTimes; epoch++) {
		res = 0;
		count_my_negatives=0;
		for (INT batch = 0; batch < nbatches; batch++) {
			pthread_t *pt = (pthread_t *)malloc(threads * sizeof(pthread_t));
			for (long a = 0; a < threads; a++)
				pthread_create(&pt[a], NULL, trainMode,  (void*)a);
			for (long a = 0; a < threads; a++)
				pthread_join(pt[a], NULL);
			free(pt);
			memcpy(relationVec, relationVecDao, dimensionR * relationTotal * sizeof(REAL));
			memcpy(entityVec, entityVecDao, dimension * entityTotal * sizeof(REAL));
			memcpy(matrix, matrixDao, dimension * relationTotal * dimensionR * sizeof(REAL));
		}
		printf("epoch %d %f\n", epoch, res);
		printf("count my negatives: %d \n", count_my_negatives);
	}
}
/*
	Get the results of transR.
*/
void out_binary() {
		INT len, tot;
		REAL *head;		
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".bin").c_str(), "wb");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".bin").c_str(), "wb");
		len = relationTotal * dimension; tot = 0;
		head = relationVec;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f2);
			tot = tot + sum;
		}
		len = entityTotal * dimension; tot = 0;
		head = entityVec;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f3);
			tot = tot + sum;
		}	
		fclose(f2);
		fclose(f3);
		FILE* f1 = fopen((outPath + "A" + note + ".bin").c_str(), "wb");
		len = relationTotal * dimension * dimensionR; tot = 0;
		head = matrix;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f1);
			tot = tot + sum;
		}
		fclose(f1);
}

void out() {
		if (outBinaryFlag) {
			out_binary(); 
			return;
		}
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".vec").c_str(), "w");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".vec").c_str(), "w");
		for (INT i = 0; i < relationTotal; i++) {
			INT last = dimension * i;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f2, "%.6f\t", relationVec[last + ii]);
			fprintf(f2,"\n");
		}
		for (INT  i = 0; i < entityTotal; i++) {
			INT last = i * dimension;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f3, "%.6f\t", entityVec[last + ii] );
			fprintf(f3,"\n");
		}
		fclose(f2);
		fclose(f3);
		FILE* f1 = fopen((outPath + "A" + note + ".vec").c_str(),"w");
		for (INT i = 0; i < relationTotal; i++)
			for (INT jj = 0; jj < dimension; jj++) {
				for (INT ii = 0; ii < dimensionR; ii++)
					fprintf(f1, "%f\t", matrix[i * dimensionR * dimension + jj + ii * dimension]);
				fprintf(f1,"\n");
			}
		fclose(f1);
}


/*
	Main function
*/

int ArgPos(char *str, int argc, char **argv) {
	int a;
	for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
		if (a == argc - 1) {
			printf("Argument missing for %s\n", str);
			exit(1);
		}
		return a;
	}
	return -1;
}

void setparameters(int argc, char **argv) {
	int i;
		if ((i = ArgPos((char *)"-number_iteration", argc, argv)) > 0) number_of_iteration = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-percentage_negatives_generated", argc, argv)) > 0) percentage_of_negatives_generated = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-use_ontology", argc, argv)) > 0) {std::string ontologyValue = argv[i + 1];use_ontology = (ontologyValue == "true" || ontologyValue == "1");}
		if ((i = ArgPos((char *)"-use_structure", argc, argv)) > 0) {std::string heuristicsValue = argv[i + 1]; use_structure = (heuristicsValue == "true" || heuristicsValue == "1");}
		if ((i = ArgPos((char *)"-size", argc, argv)) > 0) dimension = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-input", argc, argv)) > 0) inPath = argv[i + 1];
		if ((i = ArgPos((char *)"-output", argc, argv)) > 0) outPath = argv[i + 1];
		if ((i = ArgPos((char *)"-load", argc, argv)) > 0) loadPath = argv[i + 1];
		if ((i = ArgPos((char *)"-thread", argc, argv)) > 0) threads = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-epochs", argc, argv)) > 0) trainTimes = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-nbatches", argc, argv)) > 0) nbatches = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-alpha", argc, argv)) > 0) alpha = atof(argv[i + 1]);
		if ((i = ArgPos((char *)"-margin", argc, argv)) > 0) margin = atof(argv[i + 1]);
		if ((i = ArgPos((char *)"-note", argc, argv)) > 0) note = argv[i + 1];
		if ((i = ArgPos((char *)"-old_note", argc, argv)) > 0) old_note = argv[i + 1];

		cout << "End setparameters. \n";
}

int main(int argc, char **argv) {
	setparameters(argc, argv);
	init();
	if (loadPath != "") load();
	train(NULL);
	if (outPath != "") out();
	return 0;
}



//-------------------------------------------------------------------------------------------------------------------
//---------------------------- IMPLEMENTATIONS OF THE FUNCTIONS FOR THE STRUCTURE-BASED NEG.-------------------------
//-------------------------------------------------------------------------------------------------------------------

Triple findReplacement(int h, int r, int t, int postriple_index){
	Triple negative_to_create;

	//decide if i want to replace head or tail
	int entityToReplace;
	int pr;
	if (bernFlag)pr = 1000 * right_mean[trainList[postriple_index].r] / (right_mean[trainList[postriple_index].r] + left_mean[trainList[postriple_index].r]);
	else pr = 500;
	if(randd(0) % 1000 < pr){
		entityToReplace=h;
	}
	else{
		entityToReplace=t;
	}

	int found;
	int count=0;

    int substitute;

	//check if the entity i want to replace has (enough) neighbors
	//pthread_mutex_lock(&myMutex);
	if(mapNeighbors[entityToReplace].size()==0||testineH_neigh[postriple_index]==-1||testineT_neigh[postriple_index]==-1){							//entity doesn't have neighbors --> random negative
		negative_to_create.h=-1;negative_to_create.r=-1; negative_to_create.t=-1;
		return negative_to_create;
	}



	if(entityToReplace==h){
		//Retrieve the last neighbor of this entity that i used for this positive triple
        int i=testineH_neigh[postriple_index];
		//pthread_mutex_unlock(&myMutex);

		if( mapNeighbors[entityToReplace].size()>0 && i>=0){
			//find a neighbors that i haven't already used to change this entity (substitute!=INT16_MIN) and that doesnt cresate a negative alreadddy used or a positive (adjacencyList_all_trainTriples.get(substitute,t,r)=false)
			do{
				i++;
				substitute=mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i];
			}while((substitute==INT16_MIN || adjacencyList_all_trainTriples.get(substitute,t,r)) && mapNeighbors[entityToReplace].size()-i>0);
		}
		//if i couldnt find a suitable neighbors


        if(i<0 || mapNeighbors[entityToReplace].size()-i==0){
            found=0;
			testineH_neigh[postriple_index]=-1;
        }
		else{ 		//if i found a suitable neighbor
			found=1;
			testineH_neigh[postriple_index]=i;
			//important: i change the neighbor used in INT16_MIN so that i won't use it again
			mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i]=INT16_MIN;
		}


	}
	else{
		//Retrieve the last neighbor of this entity that i used for this positive triple
		int i=testineT_neigh[postriple_index];

        if( mapNeighbors[entityToReplace].size()>0 && i>=0){
			//find a neighbors that i haven't already used to change this entity (substitute!=INT16_MIN) and that doesnt cresate a negative alreadddy used or a positive (adjacencyList_all_trainTriples.get(substitute,t,r)=false)
            do{
				i++;
                substitute=mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i];
            }
            while((substitute==INT16_MIN || adjacencyList_all_trainTriples.get(h,substitute,r)) && mapNeighbors[entityToReplace].size()-i>0);
        }
		

		//if i couldnt find a suitable neighbors

        if(i<0 || mapNeighbors[entityToReplace].size()-i==0){				
            found=0;
			testineT_neigh[postriple_index]=-1;
        }
		else{			//if i found a suitable neighbor
			found=1;
			testineT_neigh[postriple_index]=i;
			//important: i change the neighbor used in INT16_MIN so that i won't use it again
			mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i]=INT16_MIN;
		}


    }

	//if i don't find a negative that hasn't already been created --> random negative
	if(found==0){
		negative_to_create.h=-1;negative_to_create.r=-1; negative_to_create.t=-1;
		return negative_to_create;
	}

	//compose the negative and add it to the 3d matrix
	negative_to_create.r=r; 
	if(entityToReplace==h){
		negative_to_create.h=substitute;
		negative_to_create.t=t;
		//pthread_mutex_lock(&myMutex);
		addEdge(substitute,r,t);
		//pthread_mutex_unlock(&myMutex);
	}
	else{
		negative_to_create.h=h;
		negative_to_create.t=substitute;
		//pthread_mutex_lock(&myMutex);
		addEdge(h,r,substitute);
		//pthread_mutex_unlock(&myMutex);
	}


	return negative_to_create;
}

//-------------------------------------------------------------------------------------------------------------------
//---------------------------- IMPLEMENTATIONS OF THE FUNCTIONS FOR THE ONTOLOGY-BASED NEG.--------------------------
//-------------------------------------------------------------------------------------------------------------------

//from positive (x,p,y) create negative (entity of classes disjointed with y,p,y)
Triple change_head_disjointWith(int idx_pos) {
    Triple myneg;
    myneg.h = -1;
    myneg.r = -1;
    myneg.t = -1;

    int h = trainList[idx_pos].h;
    int r = trainList[idx_pos].r;
    int t = trainList[idx_pos].t;

	int tries=0;

	//if the tail has disjointed classes
	if (mapDisjointClasses.find(t) != mapDisjointClasses.end()) {

			//find the max num of entities that the disjointed classes have & intialize mapUsedClasses_disj_h, structure used to speed up the search of entity to be used
			int max_num_entity=0;
			bool first_time=false;
			if(mapUsedClasses_disj_h[idx_pos].size()==0)first_time=true;
			for (const auto& current_class : mapDisjointClasses_for_head_change[t]) {
				if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
				if(first_time)mapUsedClasses_disj_h[idx_pos].push_back(1);
			}


			//search the entity between the disjointed classes in a rotative way: if i have 3 classes disjointed e.g. A,B,C first im going to try to substitute with an entity from A, then B then C and then A again
			int index_wanted_class;
			int wanted_class;
			int index_wanted_entity;
			int wanted_entity;
			while(myneg.h==-1){
				//ran out of entities to try
				if(testineH_ontology[idx_pos]>=(max_num_entity*mapDisjointClasses_for_head_change[t].size())|| tries>SlovinTries){
					break;
				}

				//identify which class i have to try (A or B or C) based on the last one used to corrupt this positive triple (contained in testineH_ontology[idx_pos])
				index_wanted_class = testineH_ontology[idx_pos]%mapDisjointClasses_for_head_change[t].size();
				wanted_class=mapDisjointClasses_for_head_change[t][index_wanted_class];
				index_wanted_entity= testineH_ontology[idx_pos]/mapDisjointClasses_for_head_change[t].size();

			
				//if the class i wanted to try an entity from ran out of entities i skip to the next one and i use the structure mapUsedClasses_disj_h so that next time i'm not trying the class that ran out of entities and i'll save time
				if(mapClassesToEntities[wanted_class].size()<=index_wanted_entity){
					testineH_ontology[idx_pos]+=mapUsedClasses_disj_h[idx_pos][index_wanted_class];
					int index_temp=index_wanted_class;bool exit;
					do{
						index_temp=index_temp-1;
						if(index_temp==-1)index_temp=mapDisjointClasses_for_head_change[t].size()-1;
						if(mapUsedClasses_disj_h[idx_pos][index_temp]==0){
							exit=false;
						}
						else{
							mapUsedClasses_disj_h[idx_pos][index_temp]+=mapUsedClasses_disj_h[idx_pos][index_wanted_class];
							exit=true;
						}
					}while(exit==false);
					mapUsedClasses_disj_h[idx_pos][index_wanted_class]=0;

					continue;
				}

				//identify the entity that i want to try to substitute
				wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];
				//see if the negative constructed with it has been already been used
				if(adjacencyList_all_trainTriples.get(wanted_entity,t,r)==true){
					tries++;
					testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_disj_h[idx_pos][index_wanted_class];
					continue;
				}

				//compose the negative
				testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_disj_h[idx_pos][index_wanted_class];
				myneg.h = wanted_entity;
				myneg.r = r;
				myneg.t = t;
			}


			//not found and can't try again for this positive triple
			if(myneg.h==-1){testineH_ontology[idx_pos]=INT16_MIN;}
		}
		else{
			//not found and can't try again for this positive triple
			if(myneg.h==-1){testineH_ontology[idx_pos]=INT16_MIN;}
		}

    return myneg;
}
//from positive (x,p,y) create negative (x,p,entity of classes disjointed with classes(x))
Triple change_tail_disjointWith(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;

	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;



	//find all the classes of my head (if there are)
	if(mapEntitiesToClasses.find(h)!=mapEntitiesToClasses.end()){
		

		//for each class of the head, collect the classes that are disjointed with that one
		set<int> classes_disjointed;
		const vector<int>& entities_classes = mapEntitiesToClasses[h];


		//find all the disjointed classes to my classes
		for(const auto& current_class:entities_classes){
			if(mapDisjointClasses.find(current_class)!= mapDisjointClasses.end()){
				classes_disjointed.insert(mapDisjointClasses[current_class].begin(),mapDisjointClasses[current_class].end());
			}
		}
		

		std::vector<int> classes_disjointed_vector(classes_disjointed.begin(), classes_disjointed.end());

		//if there are, take them one by one, checking if they have already been used with the counter in testineT_ont
		int counter=0;
		if(classes_disjointed.size()>0){
			if(testineT_ontology[idx_pos]<classes_disjointed_vector.size()){
				myneg.h=h;
				myneg.r=r;
				myneg.t=classes_disjointed_vector[testineT_ontology[idx_pos]];
				testineT_ontology[idx_pos]=testineT_ontology[idx_pos]+1;
				if(adjacencyList_all_trainTriples.get(myneg.h,myneg.t,myneg.r)==true){
					myneg.h=-1;myneg.r=-1;myneg.t=-1;
				}
			}
			else{
				testineT_ontology[idx_pos]=INT16_MIN;
			}
		}
	}
	else{
		testineT_ontology[idx_pos]=INT16_MIN;
	}

	
	return myneg;
}
//from positive (x,p,y) create negative (entity of classes not in the domain of p,p,y)
Triple change_domain(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;
	int tries=0;


	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;

	//if the relation has domain classes
	if(mapDomains.find(r)!=mapDomains.end()){

			std::vector<int> not_admitted_classes=mapNotAdmitted_domain[r];

			//find the max num of entities that the not admitted classes have & intialize mapUsedClasses_domain, structure used to speed up the search of entity to be used

			int max_num_entity=0;
			bool first_time=false;
			if(mapUsedClasses_domain[idx_pos].size()==0)first_time=true;
			for (const auto& current_class : not_admitted_classes) {
				if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
				if(first_time)mapUsedClasses_domain[idx_pos].push_back(1);
			}


			//search the entity between the not admitted classes in a rotative way: if i have 3 classes not admitted e.g. A,B,C first im going to try to substitute with an entity from A, then B then C and then A again
			int index_wanted_class;
			int wanted_class;
			int index_wanted_entity;
			int wanted_entity;
			while(myneg.h==-1){
				//ran out of entities to try
				if(testineH_ontology[idx_pos]>=(max_num_entity*not_admitted_classes.size())||tries>SlovinTries){
					break;
				}
				//identify which class i have to try (A or B or C) based on the last one used to corrupt this positive triple (contained in testineH_ontology[idx_pos])
				index_wanted_class = testineH_ontology[idx_pos]%not_admitted_classes.size(); 
				wanted_class=not_admitted_classes[index_wanted_class];
				index_wanted_entity= testineH_ontology[idx_pos]/not_admitted_classes.size();
				
				//if the class i wanted to try an entity from ran out of entities i skip to the next one and i use the structure mapUsedClasses_domain so that next time i'm not trying the class that ran out of entities and i'll save time
				if(mapClassesToEntities[wanted_class].size()<=index_wanted_entity){

					testineH_ontology[idx_pos]+=mapUsedClasses_domain[idx_pos][index_wanted_class];;
					int index_temp=index_wanted_class;bool exit;
					

					do{
						index_temp=index_temp-1;
						if(index_temp==-1)index_temp=not_admitted_classes.size()-1;
						if(mapUsedClasses_domain[idx_pos][index_temp]==0){
							exit=false;
						}
						else{
							mapUsedClasses_domain[idx_pos][index_temp]+=mapUsedClasses_domain[idx_pos][index_wanted_class];
							exit=true;
						}
					}while(exit==false);
					mapUsedClasses_domain[idx_pos][index_wanted_class]=0;
					
					continue;
				}
				//identify the entity that i want to try to substitute
				wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];
				//see if the negative constructed with it has been already been used
				if(adjacencyList_all_trainTriples.get(wanted_entity,t,r)==true){
					testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_domain[idx_pos][index_wanted_class];
					tries++;
					continue;
				}
				//compose the negative
				testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_domain[idx_pos][index_wanted_class];			
				myneg.h = wanted_entity;
				myneg.r = r;
				myneg.t = t;
			}
			//not found and can't try again for this positive triple
			if(myneg.h==-1)testineH_ontology[idx_pos]=INT16_MIN;	
		
	}
		else{
			//not found and can't try again for this positive triple
			testineH_ontology[idx_pos]=INT16_MIN;	
		}


	return myneg;
}
//from positive (x,p,y) create negative (x,p,entity of classes not in the range of p)
Triple change_range(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;
	int tries=0;

	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;

	//if the relation has range classes
	if(mapRanges.find(r)!=mapRanges.end()){
		
		std::vector<int> not_admitted_classes=mapNotAdmitted_range[r];


		//find the max num of entities that the not admitted classes have & intialize mapUsedClasses_range, structure used to speed up the search of entity to be used
		int max_num_entity=0;
		bool first_time=false;
		if(mapUsedClasses_range[idx_pos].size()==0)first_time=true;
		for (const auto& current_class : not_admitted_classes) {
			if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
			if(first_time)mapUsedClasses_range[idx_pos].push_back(1);
		}


		//search the entity between the not admitted classes in a rotative way: if i have 3 classes not admitted e.g. A,B,C first im going to try to substitute with an entity from A, then B then C and then A again
		int index_wanted_class;
		int wanted_class;
		int index_wanted_entity;
		int wanted_entity;
		//if the class i wanted to try an entity from ran out of entities i skip to the next one and i use the structure mapUsedClasses_range so that next time i'm not trying the class that ran out of entities and i'll save time
		while(myneg.h==-1){
			if(testineT_ontology[idx_pos]>=(max_num_entity*not_admitted_classes.size())||tries>SlovinTries){
				break;
			}

			index_wanted_class = testineT_ontology[idx_pos]%not_admitted_classes.size();
			wanted_class=not_admitted_classes[index_wanted_class];
			index_wanted_entity= testineT_ontology[idx_pos]/not_admitted_classes.size();
			if(mapClassesToEntities[wanted_class].size()<=index_wanted_entity){
				testineT_ontology[idx_pos]+=mapUsedClasses_range[idx_pos][index_wanted_class];;

				int index_temp=index_wanted_class;bool exit;


				do{
					index_temp=index_temp-1;
					if(index_temp==-1)index_temp=not_admitted_classes.size()-1;

					if(mapUsedClasses_range[idx_pos][index_temp]==0){
						exit=false;
					}
					else{
						mapUsedClasses_range[idx_pos][index_temp]+=mapUsedClasses_range[idx_pos][index_wanted_class];
						exit=true;
					}
				}while(exit==false);	
								mapUsedClasses_range[idx_pos][index_wanted_class]=0;

				continue;
			}
			//identify the entity that i want to try to substitute
			wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];
			//see if the negative constructed with it has been already been used
			if(adjacencyList_all_trainTriples.get(h,wanted_entity,r)==true){
				testineT_ontology[idx_pos]=testineT_ontology[idx_pos]+mapUsedClasses_range[idx_pos][index_wanted_class];
				tries++;
				continue;
			}				
			//compose the negative
			testineT_ontology[idx_pos]=testineT_ontology[idx_pos]+mapUsedClasses_range[idx_pos][index_wanted_class];
			myneg.h = h;
			myneg.r = r;
			myneg.t = wanted_entity;
		}
		
			//not found and can't try again for this positive triple
		if(myneg.h==-1)testineT_ontology[idx_pos]=INT16_MIN;
	}
	
	else{
					//not found and can't try again for this positive triple
		testineT_ontology[idx_pos]=INT16_MIN;
	}
	
	
	return myneg;

}

Triple One_negative_creation_with_ontology(int index_positive){
	Triple mynegative;
	mynegative.h=-1;mynegative.r=-1;mynegative.t=-1;
	
    
	//DISJOINT-WITH NEGATIVES
	//if the relationship it's 'typeof' i can try to create a negative using 'DisjointWith' axioms:
	//if i want to change the head(/tail) i check if its class it's disjointed with some other class
	//if not, i try to change the tail(/head)
	int pr;
	if (bernFlag)pr = 1000 * right_mean[trainList[index_positive].r] / (right_mean[trainList[index_positive].r] + left_mean[trainList[index_positive].r]);
	else pr = 500;
	if(trainList[index_positive].r==typeOf_ID){
		if(testineT_ontology[index_positive]!=INT16_MIN && testineH_ontology[index_positive]!=INT16_MIN){
			if(randd(0) % 1000 < pr){
				mynegative=change_head_disjointWith(index_positive);
				if(mynegative.h==-1)change_head_disjointWith(index_positive);
			}
			else{
				mynegative=change_tail_disjointWith(index_positive);
				if(mynegative.h==-1)change_tail_disjointWith(index_positive);
			}
		}
		else if(testineT_ontology[index_positive]==INT16_MIN && testineH_ontology[index_positive]!=INT16_MIN){
			mynegative=change_head_disjointWith(index_positive);
		}
		else if(testineT_ontology[index_positive]!=INT16_MIN && testineH_ontology[index_positive]==INT16_MIN){
			mynegative=change_tail_disjointWith(index_positive);
		}
	}

	//DOMAIN-RANGE NEGATIVES
	//Check if i have already used the irreflexivity/asymmetry of the relation for this positive triple. If yes, i use rang/domain. If i want to change the head(/tail) i check if the relationship has a domain(/range)
	//If not, i check if it has a range(/domain)
	else{
		if(irreflexive_proprerty_to_be_used[index_positive]==true){
			mynegative.h=trainList[index_positive].h;mynegative.r=trainList[index_positive].r;mynegative.t=trainList[index_positive].h;
			irreflexive_proprerty_to_be_used[index_positive]=false;
		}
		else if(asymmetric_property_to_be_used[index_positive]==true){
			mynegative.h=trainList[index_positive].t;mynegative.r=trainList[index_positive].r;mynegative.t=trainList[index_positive].h;
			asymmetric_property_to_be_used[index_positive]=false;
		}
		else{
			if(testineT_ontology[index_positive]!=INT16_MIN && testineH_ontology[index_positive]!=INT16_MIN){
				if(randd(0) % 1000 < pr){
					mynegative=change_domain(index_positive);
					if(mynegative.h==-1)change_range(index_positive);
				}
				else{
					mynegative=change_range(index_positive);
					if(mynegative.h==-1)change_domain(index_positive);
				}
			}
			else if(testineT_ontology[index_positive]==INT16_MIN && testineH_ontology[index_positive]!=INT16_MIN){
				mynegative=change_domain(index_positive);
			}
			else if(testineT_ontology[index_positive]!=INT16_MIN && testineH_ontology[index_positive]==INT16_MIN){
				mynegative=change_range(index_positive);
			}
		}
	}

	//pthread_mutex_lock(&myMutex);
	if(mynegative.h!=-1){addEdge(mynegative.h,mynegative.r,mynegative.t);}
	//pthread_mutex_unlock(&myMutex);

	return mynegative;
}


//-------------------------------------------------------------------------------------------------------------------
//---------------------------- IMPLEMENTATIONS OF THE FUNCTIONS FOR ITERATIONS>1-------------------------------------
//-------------------------------------------------------------------------------------------------------------------

/*
	function to match the positive with the negative misclassified in the previous iteration if it exists.
*/
Triple find_Negative_from_previous_iteration(int index_positive){
	Triple res;
	res.h=-1;res.r=-1;res.t=-1;
	std::map<int, std::vector<Triple>>::iterator it = prevIter_negativesList.find(index_positive);
	if (it != prevIter_negativesList.end()) {
		// L'indice i esiste come chiave nella mappa
		std::vector<Triple>& triples = it->second;
		if (!triples.empty()) {
				res = triples.back();
				triples.pop_back();
			if(triples.empty()) {
				prevIter_negativesList.erase(it);
				res.h=-1;res.r=-1;res.t=-1;
			}
		}
		else{
			prevIter_negativesList.erase(it);
		}
	} 


	return res;
}
/*
	function to find an entity with cosine similarity (of the embedding) <0
*/
int find_not_similar(int entity){
	int substitute=rand_max(0,entityTotal-1);
	while(computeSimilarity(getEmbeddingforEntity(entity),getEmbeddingforEntity(substitute))>0){
		substitute=rand_max(0,entityTotal-1);
	}
	return substitute;
}





