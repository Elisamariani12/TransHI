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
- Remove Inconsitencies - The file "complete_graph" has to be in the same folder as the files "train.txt","test.txt","valid.txt". This step clean the complete graph from the inconsistencies and then split the triples based on the division done by the authors of TransOWL (https://github.com/Keehl-Mihael/TransROWL-HRS). The files with the consistent triples produced are: "train2id_Consistent.txt", "test2id_Consistent.txt", "valid2id_Consistent.txt".
```bash
   javac -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar InconsistencyCorrection.java
   
   java -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar InconsistencyCorrection "/path_to_ontology_file_complete_graph/" "/path_to_ontology/ontology_file.ttl"
```
2. Ontology Axioms Entailment - In this step, the files "relation2id" and "entity2id" containing all the entities and relationships of the KG. This step does the entailment of ontological axioms and store the axioms explicitely contained in the KG and the ones entailed in files: "SuperClasses_axioms.txt", "SubClasses_axioms.txt", "SuperProperties_axioms.txt", ... .
   ```bash
   javac -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar Axiom_entailment.java
   java -cp ./:./org.semanticweb.HermiT.jar:./HermiT.jar Axiom_entailment "/path_to_files_entity2id_and_relation2id/" "/path_to_ontology/ontology_file.ttl"
   ```
3. Positive Triples Augmentation -  "path_to_axioms" è il percorso per i files creati nello step precedente: "SuperClasses_axioms.txt", "SubClasses_axioms.txt", "SuperProperties_axioms.txt", ... . The file containing the train triples augmented with the newly created ones is "train2id_Consistent_withAugmentation.txt". 
   ```bash
javac PositiveTripleAugmentation.java
 java PositiveTripleAugmentation "/path_to_axioms/" "/path_to_train2id_consistent/" "/path_to_relation2id/"
 ```

