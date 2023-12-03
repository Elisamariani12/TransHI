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


INT typeOf_ID;
INT number_negatives_per_positive=1000; 	//because of the standard 1000 epochs of training
INT number_creation=0;

INT count_my_negatives=-1;

//PATH/NOTE initialized to ""
string inPath = "";
string outPath = "";

std::ofstream outputFile;

//structure to save a triple
struct Triple {
	INT h, r, t;
};

struct Edge {
  int tail;
  int relation;
};

//structures to save training data
Triple  *trainList,*negativesList;

//NEEDED TO HANDLE THE ONTOLOGY-BASED NEGATIVES GENERATION
std::map<int, std::vector<int>> mapDisjointClasses;
std::map<int, std::vector<int>> mapDomains;
std::map<int, std::vector<int>> mapRanges;
std::map<int, std::vector<int>> mapClassesToEntities;
std::map<int, std::vector<int>> mapEntitiesToClasses;
std::map<int, std::vector<int>> mapSuperClasses;
std::map<int, std::vector<int>> mapUsedClasses_range;
std::map<int, std::vector<int>> mapUsedClasses_domain;
std::map<int, std::vector<int>> mapUsedClasses_disj_h;
vector<bool> asymmetric_property_to_be_used;
vector<bool> irreflexive_proprerty_to_be_used;
std::map<int, std::vector<int>> mapNotAdmitted_domain;
std::map<int, std::vector<int>> mapNotAdmitted_range;
vector<int> LastIter_found;
vector<int> testineT_ontology;
vector<int> testineH_ontology;
vector<bool> entity_w_class;

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



//numbers of relations,entity, train triples
INT relationTotal, entityTotal, tripleTotal;
/*
	function to add negatives/training triples to the 3d matrix
*/
void addEdge(int from, int relation, int to) {
		adjacencyList_all_trainTriples.set(from,to,relation);
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

	entity_w_class.resize(entityTotal,false);
	for(int i=0;i<entityTotal;i++){
		if(mapEntitiesToClasses.find(i)!=mapEntitiesToClasses.end()){
			entity_w_class[i]=true;
		}
	}
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
        //if i have superclasses of my classes i include them in the allowed classes because a lot of entities that are allowed in the domain
		// are also in relationship "type" with the superclass (that could be not included in the dom)
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


        //if i have superclasses of my classes i include them in the allowed classes because a lot of entities that are allowed in the range
		// are also in relationship "type" with the superclass (that could be not included in the range)
		int count_to_debug=0;
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
}
/*
	shuffle randomly the vectors in a map<int,vector<int>>
*/
void shuffleVectorsInMap(std::map<int, std::vector<int>>& myMap) {
    int myseed = 12345;

    for (auto& pair : myMap) {
        std::mt19937 generator(myseed + pair.first); // Varia il seed in base alla chiave
        std::shuffle(pair.second.begin(), pair.second.end(), generator);
    }
}
/*
	initialize the structures with the traintriples and with the axioms for ontology-based negatives
*/
void init() {
	
	// Retrieve the ID related to the 'type' relationship
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
	fclose(fin);
	// initialize the embeddings of relations randomly

	fin = fopen((inPath+"entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &entityTotal);
	fclose(fin);
	//set up the structure with the training triples, also counting the frequence of entities and relations

	fin = fopen((inPath+"train2id_Consistent_withAugmentation.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &tripleTotal);
	trainList = (Triple *)calloc(tripleTotal, sizeof(Triple));
	for (INT i = 0; i < tripleTotal; i++) {
		tmp = fscanf(fin, "%d", &trainList[i].h);
		tmp = fscanf(fin, "%d", &trainList[i].t);
		tmp = fscanf(fin, "%d", &trainList[i].r);
	}
	fclose(fin);

	negativesList = (Triple *)calloc(tripleTotal, sizeof(Triple));


	/*-----------------------ADDITIONAL PART TO ALLOCATE THE ADJACENCY MATRIX AND POPULATE IT --------------------------------------*/


	cout<<"populating adjacency list : ";
	//populate the adjacency list
	for(int i=0;i<tripleTotal;i++){
		addEdge(trainList[i].h,trainList[i].r,trainList[i].t);
	}
	cout<<"done \n";

	
	/*ADDITIONAL PART FOR LOADING THE INPUT FILES WITH THE ONTOLOGY AXIOMS NEEDED TO BUILD THE NEGATIVES --------------------------------*/
		// load disjoint classes axioms
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
	shuffleVectorsInMap(mapDisjointClasses);
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
	shuffleVectorsInMap(mapDomains);
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
	shuffleVectorsInMap(mapRanges);
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
	shuffleVectorsInMap(mapSuperClasses);
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


	/*----------------------------------------------------------------------------------------------------------------------*/	

	/*--ADDITIONAL PART TO ALLOCATE SPACE FOR SUPPORT STRUCTURES NEEDED TO AVOID REPEATING CHECKS OVER NEGATIVE TRIPLES------*/
	testineH_ontology.resize(tripleTotal,0);
	testineT_ontology.resize(tripleTotal,0);
	LastIter_found.resize(tripleTotal,1);
	cout<<"done \n";

	/*------------------------------------------------------------------------------------------------------------------------------*/

	
  cout << "End init \n";
}




//---------------------------PROTOYPE OF THE FUNCTION to create negatives ----------------------------

Triple One_negative_creation_with_ontology(int index_positive);



/*
	------------------------------print the inconsistent triples in a file----------------------------
*/


void out() {
	int count=0;
	for (int i = 0; i < tripleTotal; i++) {
		if (negativesList[i].h != -1) {
			outputFile <<i<<" "<<negativesList[i].h << " " << negativesList[i].t << " " << negativesList[i].r << std::endl;
			count++;
		}
	}
	for (int i = 0; i < tripleTotal; i++) {
		negativesList[i].h=-1;negativesList[i].r=-1;negativesList[i].t=-1;
	}

	std::cout <<count<< " triples printed in file  InconsistentTriples"<<std::to_string(number_creation)<<".txt" << std::endl;
}
//----------------------------------------------------------------------------------------------------------


void* create() {
  
  /*while there are new inconsistent triple in the file produced, keep creating them*/
  while(count_my_negatives!=0){
		count_my_negatives=0;
		number_creation++;
		outputFile.open(outPath+"InconsistentTriples_"+std::to_string(number_creation)+".txt");

		for(int round=0;round<number_negatives_per_positive;round++){
			cout<<"Round:"<<round<<"\n";
			for (INT k = tripleTotal-1; k >= 0; k--) {
				if(LastIter_found[k]==1){
				// NEGATIVES CREATION WITH ONTOLOGY
					Triple mynegative=One_negative_creation_with_ontology(k);
					if(mynegative.h!=-1)count_my_negatives++;
					negativesList[k]=mynegative;
				}
			}
			out();
		}
		outputFile.close();
  	}
}
/*
	function to retrieve main parameters
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
		if ((i = ArgPos((char *)"-input", argc, argv)) > 0) inPath = argv[i + 1];
		if ((i = ArgPos((char *)"-out", argc, argv)) > 0) outPath = argv[i + 1];
		cout << "End setparameters. \n";
}


/*
	MAIN
*/

int main(int argc, char **argv) {
	setparameters(argc, argv);
	init();
	create();
	return 0;
}


//-------------------------------------------------------------------------------------------------------------------
//---------------------------- METHODS TO CREATE NEGATIVES ----------------------------------------------------------
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
			bool found;
			//scan all the entities and for each one check that the negative that would be created with it 1)is not in the training set 2)has not already been created
			do{
				found=false;
				int curr_entity_to_check=testineH_ontology[idx_pos];
				if(adjacencyList_all_trainTriples.get(curr_entity_to_check,r,t)==false){
					if(entity_w_class[curr_entity_to_check]==true){
						for(auto& class_substitute:mapEntitiesToClasses[curr_entity_to_check]){
							if(find(mapDisjointClasses[t].begin(),mapDisjointClasses[t].end(),class_substitute)!=mapDisjointClasses[t].end()){
								found=true;
								myneg.h=curr_entity_to_check;myneg.r=r;myneg.t=t;
							}
						}
					}
				}
				testineH_ontology[idx_pos]++;
			}while(found==false && testineH_ontology[idx_pos]<entityTotal);
			//not found and can't try again for this positive triple
			if(found==false)testineH_ontology[idx_pos]=INT16_MIN;
		}
		else{
			//not found and can't try again for this positive triple
			testineH_ontology[idx_pos]=INT16_MIN;
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

			//scan all the entities and for each one check that the negative that would be created with it 1)is not in the training set 2)has not already been created
			bool found;
			do{
				found=false;
				int curr_entity_to_check=testineH_ontology[idx_pos];
				if(adjacencyList_all_trainTriples.get(curr_entity_to_check,r,t)==false){
					if(entity_w_class[curr_entity_to_check]==true){
						for(auto& class_substitute:mapEntitiesToClasses[curr_entity_to_check]){
							if(std::find(not_admitted_classes.begin(),not_admitted_classes.end(),class_substitute)!=not_admitted_classes.end()){
								found=true;
								myneg.h=curr_entity_to_check;myneg.r=r;myneg.t=t;
							}
						}
					}
				}
				testineH_ontology[idx_pos]++;
			}while(found==false && testineH_ontology[idx_pos]<entityTotal);
		//not found and can't try again for this positive triple
		if(found==false)testineH_ontology[idx_pos]=INT16_MIN;
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
		
		//scan all the entities and for each one check that the negative that would be created with it 1)is not in the training set 2)has not already been created
		bool found;
		do{
			found=false;
			int curr_entity_to_check=testineT_ontology[idx_pos];
			if(adjacencyList_all_trainTriples.get(h,r,curr_entity_to_check)==false){
				if(entity_w_class[curr_entity_to_check]==true){
					for(auto& class_substitute:mapEntitiesToClasses[curr_entity_to_check]){
						if(std::find(not_admitted_classes.begin(),not_admitted_classes.end(),class_substitute)!=not_admitted_classes.end()){
							found=true;
							myneg.h=h;myneg.r=r;myneg.t=curr_entity_to_check;
						}
					}
				}
			}
			testineT_ontology[idx_pos]++;
		}while(found==false && testineT_ontology[idx_pos]<entityTotal);	
		//not found and can't try again for this positive triple
		if(found==false)testineT_ontology[idx_pos]=INT16_MIN;
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
	
	if(trainList[index_positive].r==typeOf_ID){
		if(testineT_ontology[index_positive]!=INT16_MIN && testineH_ontology[index_positive]!=INT16_MIN){
			if(rand() % 2 ==0){
				mynegative=change_head_disjointWith(index_positive);
				if(mynegative.h==-1)mynegative=change_tail_disjointWith(index_positive);
			}
			else{
				mynegative=change_tail_disjointWith(index_positive);
				if(mynegative.h==-1)mynegative=change_head_disjointWith(index_positive);
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
				if(rand() % 2==0){
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


	if(mynegative.h!=-1){
		addEdge(mynegative.h,mynegative.r,mynegative.t);
		}
	else{
		LastIter_found[index_positive]=0;
	}
	return mynegative;
}
