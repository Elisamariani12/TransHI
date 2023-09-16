# TransHI
### An Iterative Pipeline for Embedding Knowledge Graphs.

This repository pertains to my thesis, the objective of which is to devise a new method for embedding knowledge graphs (KGs). The method conceived is termed "TransHI", which is an iterative pipeline that incorporates not only an innovative training algorithm but also a pre-processing phase and a mechanism to generate varied training data for use across multiple iterations of the pipeline. Specifically, the training algorithm introduces the "hybrid" aspect of this approach: negative triples are generated based either on structure or on ontological knowledge. This approach became imperative as ontologies might not cover information related to all entities and relationships within the graph. A hybrid strategy that amalgamates both ontological insights and structural aspects permits a more comprehensive utilization of the KG’s data.

The creation of TransHI, particularly its training algorithm, was inspired by the TransOWL algorithm (https://github.com/Keehl-Mihael/TransROWL-HRS). TransOWL is an embedding algorithm that leverages the ontology knowledge associated with KGs to generate negative triples. Recognizing areas for enhancement in TransOWL, a novel training algorithm was constructed.

### TransHI - overview 

The TransHI's pipeline is depicted:

![TransHI's framework](https://github.com/Elisamariani12/TransHI/blob/23615642c29334c09d8f1a020b1ffb2d448d9877/images/framework.png)

Given a KG and its corresponding ontology, the pipeline devised by TransHI can be segmented into:
- **Preprocessing phase:**
    - **Inconsistencies Correction:** To identify and remove inconsistencies in the KG.
    - **Axioms Entailment:** New axioms are entailed from the ones contained in the ontology.
    - **Positive Triples Augmentation:** New positive triples are entailed from the KG and the ontology.
        
- **Training phase:** 
    - The KG embeddings are learned using a novel training algorithm. The method to create negatives is different in the first iteration with respect to the following ones.

- **Training Data Update phase:**
    - **Inconsistent Triples Generation:** A set of inconsistent triples is generated using the KG and the ontology.
    - **Triple Classification:** The embeddings learned in the previous training are used to classify the generated inconsistent triples as positives or negatives.

The misclassified triples obtained with the classification are used as negative triples in a new training phase. The latter will be followed by yet another update phase, in an iterative manner. The process stops when, in the "Training Data Update" phase, there are no new inconsistent triples that can be generated, or when no triple is misclassified.

### TransHI - Link prediction performance

In the table below, the performance of TransHI is compared to that of TransOWL, as TransOWL was the primary algorithm targeted for improvement, and to TransE, a cornerstone among translational embedding algorithms. Their quality was assessed in the link prediction task.

![TransHI's results](https://github.com/Elisamariani12/TransHI/blob/df79e995bef58a9771070160827bc8a1d8f4c743/images/summary_table.png)

It can be observed that the best iteration of TransHI outperforms TransE and TransOWL for the considered datasets: DBPEDIA15K, YAGO, and NELL. Alongside the final result of TransHI (that of the best performance), one can also view, for comparison, the outcomes of TransE and TransOWL using preprocessed data (with preprocessing provided by TransHI). Additionally, the results from the individual use of negative triples generated based on structure and those based on ontology are displayed, as well as the outcomes of TransHI after just a single iteration.

The results for these experiments were obtained choosing the best percentage of random triples among various tries (percentages from 10 to 100 by tens). All the results related to these tries are contained in the folder "images". The parameter settings are the standard ones:
- learning rate: 0.001
- epochs: 1000
- embedding dimension: 100
- margin: 1

  
Two additional parameters, associated with the training employed by TransHI, pertain to the process of identifying entity
neighbors. A choice of k = 3 hops was made, and varying numbers of random walks were used
for the three KGs:
- DBPEDIA15k: 10,000 random walks;
- YAGO: 3,000 random walks;
- NELL: 5,000 random walks;
These choices were informed by the quantity of neighbors required for training.

### TransHI - Instructions

Instructions for executing the TransHI pipeline are provided. Specifically, the relevant codes are located in the MODELS folder and are divided into the three main phases of the pipeline: PREPROCESSING, TRAINING, and TRAINING_DATA_UPDATE.

Within the folders dedicated to the 3 KGs (DBPEDIA15K, YAGO, NELL), one can find specific files tailored for each KG. Before initiating the pipeline, one must choose a particular KG; all subsequent steps will utilize files contained within the chosen KG's folder.

In the training algorithm, triples are not employed as strings but rather as sequences of three numbers, created by assigning an identifying number to each entity and relation. Therefore, there are steps included to remove/strip the relevant IDs from the files or to convert from TTL notation (using strings) to the representation of triples with the IDs.

**PREPROCESSING - Instructions**

The initial two steps of this phase utilize an ontological reasoner, Hermit (http://www.hermit-reasoner.com/java.html), to work with ontologies. The two JAR files related to this reasoner, as used in TransHI, can be found in the 'MODELS/PREPROCESSING/' directory. To execute the commands provided below accurately, one should place the JAR files in the same directory where the programs to be run are located.

1. Inconsistencies correction


- Remove IDs from files "train2id", "test2id" e "valid2id". These files are from the authors of TransOWL (https://github.com/Keehl-Mihael/TransROWL-HRS) and are used to keep the same division train/test/valid as them. This step removes the numerical IDs from those files producing three files: "train.txt","test.txt","valid.txt".
   ```basg
   javac Triple.java
   
   javac Triple_Integer.java
   
   javac IDtoTTLconverter.java
   
   java IDtoTTLconverter "/path_to_train2id/" "train2id.txt"
   
    java IDtoTTLconverter "/path_to_test2id/" "test2id.txt"
   
    java IDtoTTLconverter "/path_to_valid2id/" "valid2id.txt"
   ```
- Remove Inconsitencies - The file "complete_graph" has to be in the same folder as the files "train.txt","test.txt","valid.txt". This step clean the complete graph from the inconsistencies and then split the triples based on the division done by the authors of TransOWL (https://github.com/Keehl-Mihael/TransROWL-HRS). The files with the consistent triples produced are: "consistent_triples_train.txt", "consistent_triples_test.txt", "consistent_triples_valid.txt".
```bash
   javac -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar InconsistencyCorrection.java
   java -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar InconsistencyCorrection "/path_to_ontology_file_complete_graph/" "/path_to_ontology/ontology_file.ttl"
```
- Reput IDs instead of the string triples in "consistent_triples_train.txt", "consistent_triples_test.txt", "consistent_triples_valid.txt" (contained in the folder with the path "path_to_files"). The files produced are : "train2id_Consistent.txt", "test2id_Consistent.txt", "valid2id_Consistent.txt".
```bash
   javac TTLtoIDconverter.java
   
   java TTLtoIDconverter "/path_to_files/" "consistent_triples_train.txt"
   
    java TTLtoIDconverter "/path_to_files/" "consistent_triples_test.txt"
   
    java TTLtoIDconverter "/path_to_files/" "consistent_triples_valid.txt"
 ```

  
2. Ontology Axioms Entailment - In this step, the files "relation2id" and "entity2id" containing all the entities and relationships of the KG have to be in the same folder . This step does the entailment of ontological axioms and store the axioms explicitely contained in the KG and the ones entailed in files: "SuperClasses_axioms.txt", "SubClasses_axioms.txt", "SuperProperties_axioms.txt", ... .
   ```bash
   javac -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar Axiom_entailment.java
   java -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar Axiom_entailment "/path_to_files_entity2id_and_relation2id/" "/path_to_ontology/ontology_file.ttl"
   ```
3. Positive Triples Augmentation -  "path_to_axioms" è il percorso per i files creati nello step precedente: "SuperClasses_axioms.txt", "SubClasses_axioms.txt", "SuperProperties_axioms.txt", ... . The file containing the train triples augmented with the newly created ones is "train2id_Consistent_withAugmentation.txt". 
```bash
javac PositiveTripleAugmentation.java
 java PositiveTripleAugmentation "/path_to_axioms/" "/path_to_train2id_consistent/" "/path_to_relation2id/"
 ```

**TRAINING - Instructions**

4. Training algorithm -first iteration
   
- CC generation - The CC for each entity of the is saved in the file "CCs.tx".
 ```bash
javac CC_Generator.java
 java CC_Generator "/path_to_train2id_Consistent_withAugmentation/"
 ```
- Neighbors generation - The first parameter is the number K of hops to find the k-hops neighbors. The second parameter is the number of random walks performed to find the k-hop neighbors. The third is the path to the file "train2id_Consistent_withAugmentation". The k-hop neighbors for each entity are saved in the file "file_Neighbors.tx".
 ```bash
javac NeighborsGenerator.java
 java NeighborsGenerator "/path_to_train2id_Consistent_withAugmentation/"
 ```
- Training algorithm -  The first parameter indicates the current iteration number. The second parameter, "percentage_negatives_generated," represents the percentage of negatives generated either by leveraging ontological knowledge or the structure. The "use_ontology" parameter should be set to true if one wishes to use ontology-based negative triples. The same applies to the "use_structure" parameter. All input files must reside in the same directory ("path_to_files"), "path_to_files", and these files include: relation2id.txt, entity2id.txt, train2id_Consistent_withAugmentation.txt, DisjointWith_axioms.txt, Domain_axioms.txt, Range_axioms.txt, SuperClasses_axioms.txt, IrreflexiveProperties_axioms.txt, AsymmetricProperties_axioms.txt, file_Neighbors.txt, and CCs.txt. The training output yields the embeddings of entities and relationships, saved in the files: relation2vec.vec and entity2vec.vec. To differentiate the embeddings generated under various settings, one can append a string to "entity2vec" and "relation2vec" using the 'note' parameter.
  ```bash
  g++ -std=c++11 TransHI.cpp -o TransHI.exe -pthread
  ./TransHI.exe -number_iteration 1 -percentage_negatives_generated 10 -use_ontology false -use_structure true -input ~/path_to_files/ -output ~/c/ -note "_10_onlyS"
   ```
  <small>*N.B. In this example 10% of the negatives triples are create using only the structure.*</small>

  

**TRAINING DATA UPDATE - Instructions**

5. Inconsistent Triples Generation - this step creates files of inconsistent triples "InconsistentTriples_1", "InconsistentTriples_2", ... . It creates as many files with inconsistent triples as possible. Each one of these files is supposed to be used in one of the subsequent iterations, therefore this code can be ran just 1 time and it will automatically produce files for all the possible iterations. The folder indicated in the parameter "input" is supposed to contain the files: relation2id.txt, entity2id.txt, train2id_Consistent_withAugmentation.txt, DisjointWith_axioms.txt, Domain_axioms.txt, Range_axioms.txt, SuperClasses_axioms.txt, IrreflexiveProperties_axioms.txt, AsymmetricProperties_axioms.txt.
  ```bash
g++ -std=c++11 InconsistentTriplesGenerator.cpp -o InconsistentTriplesGenerator.exe 
./InconsistentTriplesGenerator.exe -input ~/path_to_files/ -output ~/path_to_files/
   ```

6. Triples Classification - The first parameter is the parameter "note" used during the training algorithm so it's the string that is optionally attached to the files relation2vec and entity2vec and identifies which are the embeddings that we want to use for the classification. The "path_to_files" folder instead has to contain the files with the embeddings (entity2vec.vec,relation2vec.vec), train2id_Consistent_withAugmentation.txt, test2id_Consistent.txt and the file with the inconsistent triples followed by the number of the current iteration, f.i. for the first iteration "InconsistentTriples_1". The third parameter is the number of the current iteration. The misclassified triples are saved in a file: "inconsistent_wrongly_classified.txt".
 ```bash
python3 triple_classification.py "_10_onlyS" "/path_to_files/" "1"
 ```
 <small>*N.B. In this example the embeddings used for classification are created with 10% of the negatives using only the structure and the iteration is the first one.*</small>
 
**TRAINING NEXT ITERATIONS - Instructions**

7. Training algorithm -next iterations - The first parameter indicates the current iteration number. The second parameter, "percentage_negatives_generated" should be set at 100 because in the iterations following the first one all the misclassified triples are to be used. All input files must reside in the same directory ("path_to_files"), "path_to_files", and these files include: relation2id.txt, entity2id.txt, train2id_Consistent_withAugmentation.txt, inconsistent_wrongly_classified.txt and the files of the embeddings created in the previous training. The parameter "note" used in the previous training has to be used as parameter "old_note". Instead, the parameter "note" should have another label to identify the new embeddings created.
  ```bash
  ./TransHI.exe -number_iteration 2 -percentage_negatives_generated 100 -input ~/path_to_files/ -output ~/path_to_files/ -old_note "_10_onlyS" -note "_10_onlyS_2iter"
   ```

After the training of the iterations following the first one, another classification can follow and then another training step, in an iterative way. The pipeline stops when the file "inconsistent_wrongly_classified.txt" is empty or there are no more files "InconsistentTriples_***.txt" to use (each of them has to be used just once).

**LINK PREDICTION - Instructions**

The embeddings created in each iteration are evaluated by their performance on a link prediction task. The prediction performance can be separated between the performance in predicting "typeOf" links (the prediction of triples that have 'type' as relation) and the performance on "noTypeOf" links (the prediction of triples that do not have 'type' as relation). In the folder "path_to_embedding" there have to be the embeddings to be evaluated: the files entity2vec and relation2vec  followed by the string indicated in "note" that identify the settings of creation of the embeddings. 

For general performance: 
  ```bash
g++ -std=c++11 LinkPrediction.cpp -o LinkPrediction.exe -pthread -O3 -march=native
./LinkPrediction.exe -init ~/path_to_embeddings/ -input ~/path_to_files/ -output ~/path_to_files/ -note "_10_onlyS"
   ```

For performance on triples 'typeOf': 
  ```bash
g++ -std=c++11 LinkPrediction_TypeOf.cpp -o LinkPrediction_TypeOf.exe -pthread -O3 -march=native
./LinkPrediction_TypeOf.exe -init ~/path_to_embeddings/ -input ~/path_to_files/ -output ~/path_to_files/ -note "_10_onlyS"
   ```

For performance on triples 'noTypeOf': 
  ```bash
g++ -std=c++11 LinkPrediction_noTypeOf.cpp -o LinkPrediction_noTypeOf.exe -pthread -O3 -march=native
./LinkPrediction_noTypeOf.exe -init ~/path_to_embeddings/ -input ~/path_to_files/ -output ~/path_to_files/ -note "_10_onlyS"
   ```

 <small>*N.B. In this example the embeddings evaluated were created with 10% of the negatives.*</small>

