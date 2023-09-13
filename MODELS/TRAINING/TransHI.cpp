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

using namespace std;

const REAL pi = 3.141592653589793238462643383;

//---------------CHOOSE THE HEURISTIC -----------------------------------------
//  1- SANS (random)
//	2- SANS + highest CC
//  3- SANS + JACCARD

INT number_of_iteration;
INT choice_heuristic;
float percentage_of_negatives_generated;
bool use_heuristics;
bool use_ontology;

INT typeOf_ID=275;
INT bernFlag = 1;
INT trainTimes = 1000;
//INT trainTimes = 10;
INT nbatches = 1;
INT threads = 1;
INT dimension = 100;
REAL alpha = 0.001;
REAL margin = 1.0;
INT count_my_negatives=0;
float SlovinError=0.05f;
INT SlovinTries;


string inPath = "";
//string inPath = "../TEST_DBYAGO/";
//string outPath = "";
string outPath = "";
string loadPath = "";
string note = "";
string old_note="";

INT *lefHead, *rigHead;
INT *lefTail, *rigTail;

struct Triple {
	int h, r, t;
};

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


Triple *trainHead, *trainTail, *trainList;
std::map<int, std::vector<Triple>> prevIter_negativesList;


//NEEDED TO HANDLE THE HEURISTIC-BASED NEGATIVES GENERATION
std::vector<vector<int>> mapNeighbors;
std::map<int, std::vector<int>> mapAllRelations_outgoing;
std::map<int, std::vector<int>> mapAllRelations_incoming;
std::vector<float> mapCCs;
std::vector<float> mapDegree;
Sparse3DMatrix adjacencyList_all_trainTriples; 	


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
vector<int> testineT_neigh;
vector<int> testineH_neigh;
INT Perc_EntitiesWithNoClass;
INT Perc_EntitiesWithNoClass_nr;
INT percentage_coverage_ont;




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


/*
	There are some math functions for the program initialization.
*/
unsigned long long *next_random;

unsigned long long randd(INT id) {
	next_random[id] = next_random[id] * (unsigned long long)25214903917 + 11;
	return next_random[id];
}

INT rand_max(INT id, INT x) {
	INT result = randd(id) % x;
	while (result<0)
		result+=x;
	return result;
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

void norm(REAL * con) {
	REAL x = 0;
	for (INT  ii = 0; ii < dimension; ii++)
		x += (*(con + ii)) * (*(con + ii));
	x = sqrt(x);
	if (x>1)
		for (INT ii=0; ii < dimension; ii++)
			*(con + ii) /= x;
}

/*
	Read triples from the training file.
*/

INT relationTotal, entityTotal, tripleTotal, previous_iteration_negatives_Total;
REAL *relationVec, *entityVec;
REAL *relationVecDao, *entityVecDao;
INT *freqRel, *freqEnt;
REAL *left_mean, *right_mean;

void addEdge(int from, int relation, int to) {
		adjacencyList_all_trainTriples.set(from,to,relation);
}

bool compareByMapValue_cc(const int& a, const int& b, const std::vector<float>& mymap) {
    return mymap[a] > mymap[b];		//DECREASING order
}

bool compareByMapValue_similarity(const int& a, const int& b, const std::vector<float>& mymap) {
    return mymap[a] > mymap[b];		//decreasing order- i substitute an entity with one that has low similarity
}
bool compareByMapValue_int(const int& a, const int& b, const std::vector<int>& mymap) {
    return mymap[a] > mymap[b];		//decreasing order- i substitute an entity with one that has few relationships in common
}

std::vector<int> findNumCommonRel(int entity,vector<int> neighbors){
	//cout<<"inside findnumcommonrel \n";
	//find all the relations of my entity
	vector<int> myentityrels_out=mapAllRelations_outgoing[entity];
	vector<int> myentityrels_inc=mapAllRelations_incoming[entity];


	//create a vector of int that uses as indexes the entities_id and as values their number of common relationship with the entity
	vector<int> result;
	for(int i=0;i<neighbors.size();i++){
		vector<int> commonElements_out;
		vector<int> commonElements_inc;
		int curr_neigh=neighbors[i];

		
			std::set_intersection(myentityrels_out.begin(), myentityrels_out.end(), mapAllRelations_outgoing[curr_neigh].begin(), mapAllRelations_outgoing[curr_neigh].end(), std::back_inserter(commonElements_out));
			std::set_intersection(myentityrels_inc.begin(), myentityrels_inc.end(), mapAllRelations_incoming[curr_neigh].begin(), mapAllRelations_incoming[curr_neigh].end(), std::back_inserter(commonElements_inc));

			if(result.size()<=curr_neigh){
				result.resize(curr_neigh+1);
			}

			if(myentityrels_inc.size()+myentityrels_out.size() !=0){
				result[curr_neigh]=(commonElements_out.size()+commonElements_inc.size())/(myentityrels_inc.size()+myentityrels_out.size());
			}
			else{
				result[curr_neigh]=0.5;
			}

	}

	return result;
}
std::vector<float> getEmbeddingforEntity(int entity){
	vector<float> result(dimension);
	for(int i=0;i<dimension;i++){
		result[i]=entityVec[entity*dimension+i];
	}

	return result;
}
float normEmbedding(const std::vector<float>& v) {
    float result = 0.0f;
    for (size_t i = 0; i < v.size(); i++) {
        result += v[i] * v[i];
    }
    return std::sqrt(result);
}

float computeSimilarity(vector<float> embedding1,vector<float> embedding2){
	float result=0;
	for(int i=0;i<dimension;i++){
		result+=embedding1[i]*embedding2[i];
	}
	
	return result;
}


std::vector<float> findSimilarities(int entity,vector<int> neighbors){
	//cout<<"inside findsimil \n";
	//find all the relations of my entity
	vector<float> myembedding=getEmbeddingforEntity(entity);
	//cout<<"here 1\n";
	//create a vector of int that uses as indexes the entities_id and as values their number of common relationship with the entity
	vector<float> result;
	for(int j=0;j<neighbors.size();j++){
		int curr_neigh=neighbors[j];
		vector<float> neighbor_embedding=getEmbeddingforEntity(curr_neigh);
		//cout<<"got embedding neigh \n";
		if(result.size()<=curr_neigh){
			result.resize(curr_neigh+1);
		}
		//cout<<"pre-computesimilarity \n";
		result[curr_neigh]=computeSimilarity(myembedding,neighbor_embedding);
	}
	return result;
}
void FillMapsOfRelations(){
	for(int i=0;i<tripleTotal;i++){
		mapAllRelations_outgoing[trainList[i].h].push_back(trainList[i].r);
		mapAllRelations_incoming[trainList[i].t].push_back(trainList[i].r);
	}
}

void fillMapsOfClassesAndEntities(){
	for(int i=0;i<tripleTotal;i++){
		if(trainList[i].r==typeOf_ID){
			mapClassesToEntities[trainList[i].t].push_back(trainList[i].h);
			mapEntitiesToClasses[trainList[i].h].push_back(trainList[i].t);
		}
	}
}
void percentageEntitiesWithNoClass(){
	int count_noclass_ent=0;
	for(int i=0;i<entityTotal;i++){
		if(mapEntitiesToClasses.find(i)==mapEntitiesToClasses.end()){
			count_noclass_ent++;
		}
	}
	//if(percentage_coverage_ont>100-(double)100*count_noclass_ent/entityTotal){
	//		Perc_EntitiesWithNoClass=(double)100*count_noclass_ent/entityTotal;
	//		cout<<"Non arrotondo";
	//}
	//else{
	//	cout<<"Arrotondo per difetto";
		Perc_EntitiesWithNoClass=(double)10*count_noclass_ent/entityTotal;
		Perc_EntitiesWithNoClass=(double)10*Perc_EntitiesWithNoClass;

		Perc_EntitiesWithNoClass_nr=(double)100*count_noclass_ent/entityTotal;
	//}
	

	cout<<"The entities with no class are:"<<Perc_EntitiesWithNoClass<<" % \n";

}



void order_and_cut_map_neighbors(){
	cout<<"Sorting the neighbors based on the chosen heuristic: \n";
	switch(choice_heuristic){
		case 1:
			cout<<"the settings do not require a specific order of the neighbors.";
			break;
		case 2:
			for(int entity_id=0;entity_id<mapNeighbors.size();entity_id++){
				std::sort(mapNeighbors[entity_id].begin(), mapNeighbors[entity_id].end(), [&](const int& a, const int& b) {return compareByMapValue_cc(a, b, mapCCs);});
			}
			break;				
		case 3:
			for(int entity_id=0;entity_id<mapNeighbors.size();entity_id++){
				//cout<<"entity_corrente"<<entity_id<<" con "<<mapNeighbors[entity_id].size()<<"neighbors \n";
				if(mapNeighbors[entity_id].size()>0){
					vector<int> num_of_commonrel=findNumCommonRel(entity_id,mapNeighbors[entity_id]);
					//cout<<"dopo findnumcomon \n";
					std::sort(mapNeighbors[entity_id].begin(), mapNeighbors[entity_id].end(), [&](const int& a, const int& b) {return compareByMapValue_int(a, b, num_of_commonrel);});
          			//mapNeighbors[entity_id].erase(std::remove_if(mapNeighbors[entity_id].begin(), mapNeighbors[entity_id].end(), [&](const int& elem) {return (num_of_commonrel[elem] == 0);}), mapNeighbors[entity_id].end());
				}
			}
			break;
		
		default:
			cout<<"the settings do not require a specific order of the neighbors.";
	}
	cout<<"done \n";
}

void AddSuperClassesForGenericity_domain(){
	for(const auto& myentry:mapDomains){
        vector<int> my_super_classes;
		for(const auto& mydomain_class:myentry.second){
			if(mapSuperClasses.find(mydomain_class)!=mapSuperClasses.end()){
                my_super_classes.insert(my_super_classes.end(),mapSuperClasses[mydomain_class].begin(),mapSuperClasses[mydomain_class].end());
			}
		}
        //if i have superclasses of my classes i include them in the allowed classes because a lot of entities that are allowed in the domain
		// are also in relationship "type" with the superclass (that could be not included in the dom)
        for(const auto& superclass:my_super_classes){
            if(find(myentry.second.begin(),myentry.second.end(),superclass)==myentry.second.end()){
                mapDomains[myentry.first].push_back(superclass);
            }
        }			
	}
}
void AddSuperClassesForGenericity_range(){
	for(const auto& myentry:mapRanges){
        vector<int> my_super_classes;
		for(const auto& myrange_class:myentry.second){
			if(mapSuperClasses.find(myrange_class)!=mapSuperClasses.end()){
                my_super_classes.insert(my_super_classes.end(),mapSuperClasses[myrange_class].begin(),mapSuperClasses[myrange_class].end());
			}
		}
        //if i have superclasses of my classes i include them in the allowed classes because a lot of entities that are allowed in the range
		// are also in relationship "type" with the superclass (that could be not included in the range)
        for(const auto& superclass:my_super_classes){
            if(find(myentry.second.begin(),myentry.second.end(),superclass)==myentry.second.end()){
                mapRanges[myentry.first].push_back(superclass);
            }
        }			
	}
}

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


void	PercentageOntologyCoverage(){
	int count_ok=0;
	for(int i=0;i<tripleTotal;i++){
		if(trainList[i].r==typeOf_ID)
		{count_ok++;}
	}
	percentage_coverage_ont= (double)100*count_ok/tripleTotal;
}



void load() {
	
	FILE *fin;
	INT tmp;
	fin = fopen((inPath + "entity2vec" + old_note + ".vec").c_str(), "r");
	for (INT i = 0; i < entityTotal; i++) {
		INT last = i * dimension;
		for (INT j = 0; j < dimension; j++)
			tmp = fscanf(fin, "%f", &entityVec[last + j]);
	}
	fclose(fin);
	fin = fopen((inPath + "relation2vec" + old_note + ".vec").c_str(), "r");
	for (INT i = 0; i < relationTotal; i++) {
		INT last = i * dimension;
		for (INT j = 0; j < dimension; j++)
			tmp = fscanf(fin, "%f", &relationVec[last + j]);
	}
	fclose(fin);
}



void init() {
	
	FILE *fin;
	INT tmp;
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

	relationVec = (REAL *)calloc(relationTotal * dimension, sizeof(REAL));
	for (INT i = 0; i < relationTotal; i++) {
		for (INT ii=0; ii<dimension; ii++)
			relationVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
	}

	fin = fopen((inPath +"entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &entityTotal);
	fclose(fin);

	SlovinTries=entityTotal/((double)1+SlovinError*SlovinError*entityTotal);
	cout<<"slovin tries:"<<SlovinTries;

	entityVec = (REAL *)calloc(entityTotal * dimension, sizeof(REAL));
	for (INT i = 0; i < entityTotal; i++) {
		for (INT ii=0; ii<dimension; ii++)
			entityVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
		norm(entityVec+i*dimension);
	}

	freqRel = (INT *)calloc(relationTotal + entityTotal, sizeof(INT));
	freqEnt = freqRel + relationTotal;

	for (INT i = 0; i < entityTotal; i++) {
		freqEnt[i]=0;
	}
	fin = fopen((inPath +"train2id_Consistent_withAugmentation.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &tripleTotal);
	trainHead = (Triple *)calloc(tripleTotal, sizeof(Triple));
	trainTail = (Triple *)calloc(tripleTotal, sizeof(Triple));
	trainList = (Triple *)calloc(tripleTotal, sizeof(Triple));
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
	//int freqent_tot=0;
	//for (INT i = 0; i < entityTotal; i++) {
	//	freqent_tot+=freqEnt[i];
	//	freqEnt[i]=(trainTimes*freqEnt[i])\2;
		//cout<<"entita "<<i<<"ha frequenza"<<freqEnt[i]<<"e la media e "<<freqEnt_tot/entityTotal;
	//}
	//int freqent_avg=freqent_tot/entityTotal;

	//for (INT i = 0; i < entityTotal; i++) {
	//	if(freqEnt[i]>freqent_avg)freqEnt[i]=1;
	//	else freqEnt[i]=0;
				//cout<<"entita "<<i<<"ha frequenza"<<freqEnt[i]<<"e la media e "<<freqEnt_tot/entityTotal;

	//}
	

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
				
				//cout<<"check-match)the positive was: "<<trainList[id_curr].h<<"-"<<trainList[id_curr].r<<"-"<<trainList[id_curr].t<<"\n";
				//cout<<"check-match)the neg is: "<<prevIter_negativesList[id_curr][0].h<<"-"<<prevIter_negativesList[id_curr][0].r<<"-"<<prevIter_negativesList[id_curr][0].t<<"\n";
		}
		fclose(fin);
	}

	/*-----------------------ADDITIONAL PART TO ALLOCATE THE ADJACENCY MATRIX AND POPULATE IT --------------------------------------*/

	//populate the adjacency list
	cout<<"populating adjacency list : ";
	int i;
	for(i=0;i<tripleTotal;i++){
		//cout<<trainList[i].h<<" "<<trainList[i].t<<" "<<trainList[i].r<<"\n";
		addEdge(trainList[i].h,trainList[i].r,trainList[i].t);
	}	
	cout<<"done \n";

	

	/*----------------------------------INITIALIZE STRUCTURES NEEDED FOR STANDARD RANDOM CREATION-----------------------------------------------*/


	sort(trainHead, trainHead + tripleTotal, cmp_head());
	sort(trainTail, trainTail + tripleTotal, cmp_tail());

	lefHead = (INT *)calloc(entityTotal, sizeof(INT));
	rigHead = (INT *)calloc(entityTotal, sizeof(INT));
	lefTail = (INT *)calloc(entityTotal, sizeof(INT));
	rigTail = (INT *)calloc(entityTotal, sizeof(INT));
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

	left_mean = (REAL *)calloc(relationTotal * 2, sizeof(REAL));
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


	relationVecDao = (REAL*)calloc(dimension * relationTotal, sizeof(REAL));
	entityVecDao = (REAL*)calloc(dimension * entityTotal, sizeof(REAL));
	
	/*ADDITIONAL PART FOR LOADING THE INPUT FILES WITH THE ONTOLOGY AXIOMS NEEDED TO BUILD THE NEGATIVES --------------------------------*/
	//std::map<int16_t, std::vector<int16_t>> mapDisjointClasses;
	//std::map<int16_t, std::vector<int16_t>> mapDomains;
	//std::map<int16_t, std::vector<int16_t>> mapRanges;
	//std::map<int16_t, std::vector<int16_t>> mapClassesToEntities;
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


    /*Create a map of all the classes and the respective entities*/
	fillMapsOfClassesAndEntities();

	AddSuperClassesForGenericity_domain();
	AddSuperClassesForGenericity_range();
    CreateMapNotAdmitted_domain();
	CreateMapNotAdmitted_range();
	PercentageOntologyCoverage();
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

	


	/*----------------------------------------------------------------------------------------------------------------------*/

	/*------------if CHOSEN HEURISTIC IS SANS-CC --> LOAD CCs --------------------------------------------------------------*/
	if(choice_heuristic==2){
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
	}
	/*----------------------------------------------------------------------------------------------------------------------*/


	/*------------if CHOSEN HEURISTIC IS SANS-jaccard--> LOAD relations --------------------------------------------------------------*/
	if(choice_heuristic==3){
		cout<<"filling maps of the relations: ";
		FillMapsOfRelations();
		cout<<"done \n";
	}
	/*----------------------------------------------------------------------------------------------------------------------*/

	/*-------------------------------SECOND+ ITERATION: LOAD PREVIOUS EMBEDDINGS + HEURISTIC COSINE SIMILARITY ----------------*/
	if(number_of_iteration>1){
		load();
	}
	/*------------------------------------------------------------------------------------------------------------------------*/

	/*-----------------------ORDER AND CUT THE NEIGHBORS MAP BASED ON WHICH HEURISTIC HAS BEEN CHOSEN-----------------------*/
	if(use_heuristics==true){
	order_and_cut_map_neighbors();
	}
	/*-----------------------------------------------------------------------------------------------------------------------*/


  cout << "End init \n";
}



/*
	Training process of transE.
*/

INT Len;
INT Batch;
REAL res;

REAL calc_sum(INT e1, INT e2, INT rel) {
	REAL sum=0;
	INT last1 = e1 * dimension;
	INT last2 = e2 * dimension;
	INT lastr = rel * dimension;

	for (INT ii=0; ii < dimension; ii++)
		sum += fabs(entityVec[last2 + ii] - entityVec[last1 + ii] - relationVec[lastr + ii]);
	return sum;
}

void gradient(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimension;
	INT lastb1 = e1_b * dimension;
	INT lastb2 = e2_b * dimension;
	INT lastbr = rel_b * dimension;
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = (entityVec[lasta2 + ii] - entityVec[lasta1 + ii] - relationVec[lastar + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;
		relationVec[lastar + ii] -= x;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;
		x = (entityVec[lastb2 + ii] - entityVec[lastb1 + ii] - relationVec[lastbr + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		relationVec[lastbr + ii] -=  x;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;
	}
}

void train_kb(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b) {
	
  REAL sum1 = calc_sum(e1_a, e2_a, rel_a);
	REAL sum2 = calc_sum(e1_b, e2_b, rel_b);
  
	if (sum1 + margin > sum2) {
		res += margin + sum1 - sum2;
		gradient(e1_a, e2_a, rel_a, e1_b, e2_b, rel_b);
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
	if (tmp < trainHead[ll].t) {return tmp; }
	if (tmp > trainHead[rr].t - rr + ll - 1) {return tmp + rr - ll + 1;}
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
	INT tmp = rand_max(id, entityTotal - (rr - ll + 1)); //cout<<"id:"<<id<<" entity total: "<<entityTotal<<"rr:"<<rr<<"ll:"<<ll<<"tmp:"<<tmp<<"\n";
	if (tmp < trainTail[ll].h){ return tmp;}
	if (tmp > trainTail[rr].h - rr + ll - 1){ return tmp + rr - ll + 1;}

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

//---------------------------PROTOYPE OF THE FUNCTIONS FOR THE SINGLE HEURISTICS ----------------------------

Triple findReplacement(int h, int r, int t, int postriple_index);
std::vector<int> getNeighborsToConsider(std::vector<int> my_neighbors, std::map<int, float> mapCC_or_degree);
Triple One_negative_creation_with_ontology(int index_positive);
Triple find_Negative_from_previous_iteration(int index_positive);
int find_not_similar(int entity);




//----------------------------------------------------------------------------------------------------------


void* trainMode(void *con) {
	INT id, pr, i, j;
	id = (unsigned long long)(con);
	next_random[id] = rand();

  //cout<<"id è:"<<id<<"\n";
  /*Batch è il numero di triple in un batch*/
	for (INT k = Batch / threads; k >= 0; k--) {
		i = rand_max(id, Len);
		//cout<<"id e: "<<id<<" e len e: "<<Len;
   		
      
    	double rand_num=(double)100*rand()/RAND_MAX;

		// NEGATIVES CREATION WITH ONTOLOGY
		//i check the percentage of ontology-based negatives and try  to create a negative with the ontology
		// if it's not a positive triple for which i can use the ontology i'm going to try with heuristics (checking the percentage of usage)
		if((rand_num<percentage_of_negatives_generated && (use_ontology||use_heuristics))||(number_of_iteration>1)){
			//if i can use the ontology i'm going to use it
			double rand_num2=(double)100*rand()/RAND_MAX;

			if((use_ontology==true && number_of_iteration==1 && rand_num2>Perc_EntitiesWithNoClass)||(number_of_iteration>1 && rand_num2>Perc_EntitiesWithNoClass_nr && Perc_EntitiesWithNoClass_nr>=50)|| (number_of_iteration>1 && Perc_EntitiesWithNoClass_nr<50)){
                			//cout<<"ontology \n";

                bool found=false;
				Triple mynegative;mynegative.h=-1;mynegative.r=-1;mynegative.t=-1;
				if(number_of_iteration>1){	// try to see if i still have some negatives from previous iterations for this triple to use
					mynegative=find_Negative_from_previous_iteration(i);
					if(mynegative.h!=-1)found=true;
				}
				else{
					mynegative=One_negative_creation_with_ontology(i);
				}
				if(mynegative.h!=-1){
					train_kb(trainList[i].h, trainList[i].t, trainList[i].r, mynegative.h, mynegative.t, mynegative.r);
					norm(relationVec + dimension * trainList[i].r);
					norm(entityVec + dimension * trainList[i].h);
					norm(entityVec + dimension * trainList[i].t);
					norm(relationVec + dimension * mynegative.r);
					norm(entityVec + dimension * mynegative.h);
					norm(entityVec + dimension * mynegative.t); 
					count_my_negatives++;
					continue;
				}
			}
			// NEGATIVES CREATION WITH HEURISTICS
			// I'm going to use heuristics if the ontology based triple could not be generated (not w range/domain & not w disjointwith)
			if(number_of_iteration==1 && use_heuristics==true ){
				//cout<<"heur";
				Triple negative = findReplacement(trainList[i].h, trainList[i].r, trainList[i].t,i);
				//cout<<"pos:"<<trainList[i].h<<"-"<<trainList[i].r<<"-"<<trainList[i].t<<" neg is:"<<negative.h<<"-"<<negative.r<<"-"<<negative.t<<"\n";
				if(negative.h!=-1){ 
				train_kb(trainList[i].h, trainList[i].t, trainList[i].r, negative.h, negative.t, negative.r);
				norm(relationVec + dimension * trainList[i].r);
				norm(entityVec + dimension * trainList[i].h);
				norm(entityVec + dimension * trainList[i].t);
				norm(relationVec + dimension * negative.r);				        					      
				norm(entityVec + dimension * negative.h);
				norm(entityVec + dimension * negative.t); 
									count_my_negatives++;
				continue;
				}
			}

    	}


		// STANDARD RANDOM CREATION OF THE NEGATIVE
		//cout<<"RANDOM \n";
		//cout<<"ora scrivo tripla:"<<"\n";
		//cout<<trainList[i].h<<" "<<trainList[i].t<<" "<<trainList[i].r<<"\n";
		//if(number_of_iteration==1){
			if (bernFlag)
				pr = 1000 * right_mean[trainList[i].r] / (right_mean[trainList[i].r] + left_mean[trainList[i].r]);
			else
				pr = 500;

			if (randd(id) % 1000 < pr) {
				//cout<<"here";
				if(number_of_iteration==1){
					j = corrupt_head(id, trainList[i].h, trainList[i].r);
				}
				else {
					if(Perc_EntitiesWithNoClass_nr>=50){
						do{
							if((double)rand()/RAND_MAX<0.5) j=find_not_similar(trainList[i].t);
							else j=rand_max(0,entityTotal-1);
						}while(adjacencyList_all_trainTriples.get(trainList[i].h,j,trainList[i].r)==true || abs(calc_sum(trainList[i].h,j,trainList[i].r))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r)));
					}else{
						do{
							j=find_not_similar(trainList[i].t);
						}while(adjacencyList_all_trainTriples.get(trainList[i].h,j,trainList[i].r)==true || abs(calc_sum(trainList[i].h,j,trainList[i].r))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r)));
					}
				}
						
								//cout<<"dopo corrupt \n";

				train_kb(trainList[i].h, trainList[i].t, trainList[i].r, trainList[i].h, j, trainList[i].r);
										//	cout<<"dopo train \n";

				norm(relationVec + dimension * trainList[i].r);
				norm(entityVec + dimension * trainList[i].h);
				norm(entityVec + dimension * trainList[i].t);
				norm(entityVec + dimension * j); 
			} 
			else {
				//cout<<"hereB";
				if(number_of_iteration==1){
					j = corrupt_tail(id, trainList[i].t, trainList[i].r);
				}
				else {
						if(Perc_EntitiesWithNoClass_nr>=50){
							do{
								if((double)rand()/RAND_MAX<0.5) j=find_not_similar(trainList[i].h);
								else j=rand_max(0,entityTotal-1);
							}while(adjacencyList_all_trainTriples.get(j,trainList[i].t,trainList[i].r)==true || abs(calc_sum(j,trainList[i].t,trainList[i].r))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r)));
						}else{
							do{
								j=find_not_similar(trainList[i].h);
							}while(adjacencyList_all_trainTriples.get(j,trainList[i].t,trainList[i].r)==true || abs(calc_sum(j,trainList[i].t,trainList[i].r))<abs(calc_sum(trainList[i].h,trainList[i].t,trainList[i].r)));
						}
				}

				train_kb(trainList[i].h, trainList[i].t, trainList[i].r, j, trainList[i].t, trainList[i].r);

				norm(relationVec + dimension * trainList[i].r);
				norm(entityVec + dimension * trainList[i].h);
				norm(entityVec + dimension * trainList[i].t);
				norm(entityVec + dimension * j); 
			}
		//}
	}
  
}

void train(void *con) {
	Len = tripleTotal;
	Batch = Len / nbatches;
	next_random = (unsigned long long *)calloc(threads, sizeof(unsigned long long));
	FILE *fin;
	INT tmp;
  
	for (INT epoch = 0; epoch < trainTimes; epoch++) {
		count_my_negatives=0;

		res = 0;
		/*divide training in batches to train in different threads*/
		for (INT batch = 0; batch < nbatches; batch++) {
			    trainMode((void*)batch);
		}
		printf("epoch %d %f\n", epoch, res);
		cout<<"le negative che ho creato sono"<<count_my_negatives<<"\n";

		if(count_my_negatives<0.01*tripleTotal && number_of_iteration>1) break;
	}
}

/*
	Get the results of transE.
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
}

void out() {
		FILE* f2 = fopen((outPath+"relation2vec"+note+".vec").c_str(), "w");
		FILE* f3 = fopen((outPath+"entity2vec"+note+".vec").c_str(), "w");
	
		for (INT i=0; i < relationTotal; i++) {
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
		if ((i = ArgPos((char *)"-choice_heuristic", argc, argv)) > 0) choice_heuristic = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-percentage_negatives_generated", argc, argv)) > 0) percentage_of_negatives_generated = atoi(argv[i + 1]);
		if ((i = ArgPos((char *)"-use_ontology", argc, argv)) > 0) {std::string ontologyValue = argv[i + 1];use_ontology = (ontologyValue == "true" || ontologyValue == "1");}
		if ((i = ArgPos((char *)"-use_heuristics", argc, argv)) > 0) {std::string heuristicsValue = argv[i + 1]; use_heuristics = (heuristicsValue == "true" || heuristicsValue == "1");}

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


/*
	MAIN
*/

int main(int argc, char **argv) {
	setparameters(argc, argv);
	cout<<old_note<<"--"<<note;
	init();
	train(NULL);
	out();
	return 0;
}


//-------------------------------------------------------------------------------------------------------------------
//---------------------------- IMPLEMENTATIONS OF THE FUNCTIONS FOR THE SINGLE HEURISTICS ---------------------------
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

	//check if the entity i want to replace has (enough) neighbors
	//cout<<"the entity i want to replace is:"<<entityToReplace<<"\n";
	//std::vector<int> myNeighbors=mapNeighbors[entityToReplace];
	//cout<<"and it has"<<mapNeighbors[entityToReplace].size()<<"neighbors \n";
	if(mapNeighbors[entityToReplace].size()==0){							//entity doesn't have neighbors --> random negative
		negative_to_create.h=-1;negative_to_create.r=-1; negative_to_create.t=-1;
		return negative_to_create;
	}

	int found;
	int count=0;

	//cout<<"la size dei miei neighbors è:"<<myNeighbors.size()<<"\n";
    int substitute;
	if(entityToReplace==h){
		//cout<<"1."<<testineH_neigh[postriple_index]<<"\n";
        int i=testineH_neigh[postriple_index];
		if( mapNeighbors[entityToReplace].size()>0 && i>=0){
			//cout<<"2.\n";
			do{
				i++;
				//select last replacement entity for "entitytoreplace"
				substitute=mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i];
										//	cout<<"4.\n";

			}while(substitute==INT16_MIN || adjacencyList_all_trainTriples.get(substitute,t,r) && mapNeighbors[entityToReplace].size()-i>0);
		}

        if(i<0 || mapNeighbors[entityToReplace].size()-i==0){
            found=0;
			testineH_neigh[postriple_index]=-1;
        }
		else{
			found=1;
			testineH_neigh[postriple_index]=i;
			mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i]=INT16_MIN;
		}
	}
	else{
		int i=testineT_neigh[postriple_index];
													//cout<<"5.\n";

        if( mapNeighbors[entityToReplace].size()>0 && i>=0){
													//cout<<"6.\n";

            do{
													//cout<<"7.\n";

				i++;
                //select last replacement entity for "entitytoreplace"
                substitute=mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i];
                //if(freqEnt[entityToReplace]==1 && (double)rand()/RAND_MAX>0.5 || freqEnt[entityToReplace]==0)
            }
            while(substitute==INT16_MIN || adjacencyList_all_trainTriples.get(h,substitute,r) && mapNeighbors[entityToReplace].size()-i>0);
        }

        if(i<0 || mapNeighbors[entityToReplace].size()-i==0){
												//	cout<<"8.\n";
            found=0;
			testineT_neigh[postriple_index]=-1;
        }
		else{
			found=1;
			testineT_neigh[postriple_index]=i;
			mapNeighbors[entityToReplace][mapNeighbors[entityToReplace].size()-i]=INT16_MIN;
		}
    }

	//cout<<"i tentativi che ho fatto prima di trovarne uno ok sono: "<<count<<"\n";
	//cout<<"e io scelgo il neighbor con l'indice:"<<i;
	//if i don't find a negative that hasn't already been created --> random negative
	if(found==0){
		negative_to_create.h=-1;negative_to_create.r=-1; negative_to_create.t=-1;
		return negative_to_create;
	}


	//if(entityToReplace==53)
		//cout<<"ok i decided to change the entity 53 with the neighbor:"<<myNeighbors[i]<<" and the relation is :"<<r<<" \n";
	//if i find a negative i return it and save it in the negatives_already_created list
	negative_to_create.r=r; 
	if(entityToReplace==h){
		negative_to_create.h=substitute;
		negative_to_create.t=t;
		//removeNeighbor(h,i);
		addEdge(substitute,r,t);

	}
	else{
		negative_to_create.h=h;
		negative_to_create.t=substitute;
		//removeNeighbor(t,i);
		addEdge(h,r,substitute);
	}

	return negative_to_create;
}

Triple change_head_disjointWith(int idx_pos) {
    Triple myneg;
    myneg.h = -1;
    myneg.r = -1;
    myneg.t = -1;


    int h = trainList[idx_pos].h;
    int r = trainList[idx_pos].r;
    int t = trainList[idx_pos].t;

	int tries=0;

	if (mapDisjointClasses.find(t) != mapDisjointClasses.end()) {

			int max_num_entity=0;
			bool first_time=false;
			if(mapUsedClasses_disj_h[idx_pos].size()==0)first_time=true;
			for (const auto& current_class : mapDisjointClasses_for_head_change[t]) {
				if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
				if(first_time)mapUsedClasses_disj_h[idx_pos].push_back(1);
			}
			//if(idx_pos==106105)cout<<"maxnum_entity e:"<<max_num_entity<<"e testine deve arrivare a :"<<max_num_entity*mapDisjointClasses_for_head_change[t].size()<<"\n";


			int index_wanted_class;
			int wanted_class;
			int index_wanted_entity;
			int wanted_entity;
			//cout<<"le classi che posso usare sono:"<<mapDisjointClasses_for_head_change[t].size();
			while(myneg.h==-1){
				//if(idx_pos==106105)cout<<"testineH e:"<<testineH_ontology[idx_pos]<<"\n";
				if(testineH_ontology[idx_pos]>=(max_num_entity*mapDisjointClasses_for_head_change[t].size())|| tries>SlovinTries){
					//if(idx_pos==106105)cout<<"if1\n";
					break;
				}
				index_wanted_class = testineH_ontology[idx_pos]%mapDisjointClasses_for_head_change[t].size();
				wanted_class=mapDisjointClasses_for_head_change[t][index_wanted_class];
				index_wanted_entity= testineH_ontology[idx_pos]/mapDisjointClasses_for_head_change[t].size();
				//cout<<"index_wanted e:"<<index_wanted_entity<<"e le entita della mia classe sono"<<mapClassesToEntities[wanted_class].size()<<"\n";

				if(mapClassesToEntities[wanted_class].size()<=index_wanted_entity){
					testineH_ontology[idx_pos]+=mapUsedClasses_disj_h[idx_pos][index_wanted_class];
					//if(idx_pos==106105)cout<<"if2\n";
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
				wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];
				if(adjacencyList_all_trainTriples.get(wanted_entity,t,r)==true){
					tries++;
					testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_disj_h[idx_pos][index_wanted_class];
					//if(idx_pos==106105)cout<<"gia presente\n";
					continue;
				}
				//if(idx_pos==106105)cout<<"ok trovata";
				testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_disj_h[idx_pos][index_wanted_class];

				myneg.h = wanted_entity;
				myneg.r = r;
				myneg.t = t;
			}

			if(myneg.h==-1){testineH_ontology[idx_pos]=INT16_MIN;}
			//cout<<"counter è: "<<counter<<"\n";
		}
		else{
			if(myneg.h==-1){testineH_ontology[idx_pos]=INT16_MIN;}
		}

    return myneg;
}

Triple change_tail_disjointWith(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;

	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;



	//find all the classes of my head (if there are)
	if(mapEntitiesToClasses.find(h)!=mapEntitiesToClasses.end()){
		
		//vector<int16_t> classes_head=mapEntitiesToClasses[h];

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
					//if(idx_pos==1216||idx_pos==1217)cout<<"la negativa "<<myneg.h<<"-"<<myneg.r<<"-"<<myneg.t<<" esisteva gia \n";
					myneg.h=-1;myneg.r=-1;myneg.t=-1;
				}
			}
			else{
				testineT_ontology[idx_pos]=INT16_MIN;
			}
		}
		//cout<<"counter è: "<<counter<<"\n";

	}
	else{
		testineT_ontology[idx_pos]=INT16_MIN;
	}

	
	return myneg;
}

Triple change_domain(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;
	int tries=0;


	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;

	if(mapDomains.find(r)!=mapDomains.end()){

			std::vector<int> not_admitted_classes=mapNotAdmitted_domain[r];

			int max_num_entity=0;
			bool first_time=false;
			if(mapUsedClasses_domain[idx_pos].size()==0)first_time=true;
			for (const auto& current_class : not_admitted_classes) {
				if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
				if(first_time)mapUsedClasses_domain[idx_pos].push_back(1);
			}



			int index_wanted_class;
			int wanted_class;
			int index_wanted_entity;
			int wanted_entity;
			while(myneg.h==-1){

				if(testineH_ontology[idx_pos]>=(max_num_entity*not_admitted_classes.size())||tries>SlovinTries){
					break;
				}
				index_wanted_class = testineH_ontology[idx_pos]%not_admitted_classes.size(); 
				wanted_class=not_admitted_classes[index_wanted_class];
				index_wanted_entity= testineH_ontology[idx_pos]/not_admitted_classes.size();
				

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
				wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];
				if(adjacencyList_all_trainTriples.get(wanted_entity,t,r)==true){
					testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_domain[idx_pos][index_wanted_class];
					tries++;
					continue;
				}
				testineH_ontology[idx_pos]=testineH_ontology[idx_pos]+mapUsedClasses_domain[idx_pos][index_wanted_class];			
				myneg.h = wanted_entity;
				myneg.r = r;
				myneg.t = t;
			}

			if(myneg.h==-1)testineH_ontology[idx_pos]=INT16_MIN;	
		
	}
		else{
			testineH_ontology[idx_pos]=INT16_MIN;	
		}


	return myneg;
}

Triple change_range(int idx_pos){
	Triple myneg;
	myneg.h=-1;myneg.r=-1;myneg.t=-1;
	int tries=0;

	int h=trainList[idx_pos].h;
	int r=trainList[idx_pos].r;
	int t=trainList[idx_pos].t;

	if(mapRanges.find(r)!=mapRanges.end()){
		
		std::vector<int> not_admitted_classes=mapNotAdmitted_range[r];


		
		int max_num_entity=0;
		bool first_time=false;
		if(mapUsedClasses_range[idx_pos].size()==0)first_time=true;
		for (const auto& current_class : not_admitted_classes) {
			if(max_num_entity<mapClassesToEntities[current_class].size()) max_num_entity=mapClassesToEntities[current_class].size();
			if(first_time)mapUsedClasses_range[idx_pos].push_back(1);
		}

		int index_wanted_class;
		int wanted_class;
		int index_wanted_entity;
		int wanted_entity;

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

			wanted_entity=mapClassesToEntities[wanted_class][index_wanted_entity];

			if(adjacencyList_all_trainTriples.get(h,wanted_entity,r)==true){
				testineT_ontology[idx_pos]=testineT_ontology[idx_pos]+mapUsedClasses_range[idx_pos][index_wanted_class];
				tries++;
				continue;
			}				
			
			testineT_ontology[idx_pos]=testineT_ontology[idx_pos]+mapUsedClasses_range[idx_pos][index_wanted_class];
			myneg.h = h;
			myneg.r = r;
			myneg.t = wanted_entity;
		}
		

		if(myneg.h==-1)testineT_ontology[idx_pos]=INT16_MIN;
	}
	
	else{
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
	//If i want to change the head(/tail) i check if the relationship has a domain(/range)
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


	if(mynegative.h!=-1){addEdge(mynegative.h,mynegative.r,mynegative.t);}
	return mynegative;
}




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

int find_not_similar(int entity){
	int substitute=rand_max(0,entityTotal-1);
	while(computeSimilarity(getEmbeddingforEntity(entity),getEmbeddingforEntity(substitute))>0){
		substitute=rand_max(0,entityTotal-1);
	}
	return substitute;
}





