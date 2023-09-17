import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

import org.semanticweb.HermiT.Reasoner;
import org.semanticweb.owlapi.apibinding.OWLManager;
import org.semanticweb.owlapi.model.*;
import org.semanticweb.owlapi.reasoner.NodeSet;


class InconsistencyCorrection {
    //string of the relation that defines typeOf relationships
    public static final String typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
    public static String folder_path;   //folder with files complete_graph and the files to check: train.ttl, test.ttl, valid.ttl
    public static Integer number_set=1;
    public static Integer count_cons=0;
    public static Integer count_incons=0;


    public static void main(String[] args) throws Exception {
        folder_path=args[0];
        String name_ontology=args[1];

        //iterate the check on train,test e validation set
        for(int i=1;i<4;i++) {
            number_set=i;
            String file_to_create;
            String file_to_check;

            //0. NAME FILE TO CREATE AND TO CHECK
            if (number_set == 1){ file_to_create = "consistent_triples_train.ttl"; file_to_check="train.ttl";}
            else if (number_set == 2){ file_to_create = "consistent_triples_test.ttl";file_to_check="test.ttl";}
            else { file_to_create = "consistent_triples_valid.ttl";file_to_check="valid.ttl";}

            // 1. CREATE OWLOntologyManager object TO LOAD & SAVE ONTOLOGIES
            OWLOntologyManager manager = OWLManager.createOWLOntologyManager();

            // 2. CREATE DATA FACTORY to create axioms and WRITER to write in the file
            OWLDataFactory dataFactory = manager.getOWLDataFactory();
            BufferedWriter writer_consistent = new BufferedWriter(new FileWriter(folder_path + file_to_create));

            // 3. LOAD THE ONTOLOGY using the OWL API
            OWLOntology Real_ontology = manager.loadOntologyFromOntologyDocument(new File( name_ontology ));
            // Create a temporary copy of the ontology to not change the file
            OWLOntology ontology = manager.createOntology();
            for (OWLAxiom axiom : Real_ontology.getAxioms()) {manager.addAxiom(ontology, axiom);}
            System.out.println("ontology loaded. Axioms counted: " + ontology.getAxiomCount());

            // 4. CREATE A REASONER
            Reasoner reasoner = new Reasoner(ontology);

            // 5 FIND FOR EACH ENTITY THE CLASS OF APPARTENENCE FOR WHICH WE HAVE MORE EVIDENCE
            String[] completeGraph = Files.readAllLines(Paths.get(folder_path + "complete_graph.ttl")).toArray(new String[0]);
            //find most probable class for each entity
            Map<OWLNamedIndividual, OWLClass> mapMostProbableClasses = findMostProbableClasses(completeGraph, ontology, reasoner, manager, dataFactory);
            System.out.println("Completed detection of probable classes.");
            // Add them to the ontology. By doing so, when inconsistencies arise, the triples most likely to be incorrect based on the graph's evidence will be removed.
            System.out.println("size ontology pre-classes:"+ontology.getAxiomCount());
            List<String> axiom_added_strings=InsertProbableClassesInOntology(mapMostProbableClasses, ontology, dataFactory, manager, reasoner);
            reasoner.flush();
            System.out.println("size ontology post-classes:"+ontology.getAxiomCount());

            //6. If I have already corrected the train/test set i add them to the ontology so that the next set will be consistent also with them
            if (number_set > 1) {
                List<String> listGraph = new ArrayList<String>();
                listGraph = Files.readAllLines(Paths.get(folder_path + "consistent_triples_train.ttl"));
                String[] graph = listGraph.toArray(new String[0]);
                ontology = addLinestoOntology(graph, ontology, manager, dataFactory);
            }
            if (number_set > 2) {
                List<String> listGraph = new ArrayList<String>();
                listGraph = Files.readAllLines(Paths.get(folder_path + "consistent_triples_test.ttl"));
                String[] graph = listGraph.toArray(new String[0]);
                ontology = addLinestoOntology(graph, ontology, manager, dataFactory);
            }

            // 7. LOAD THE CANDIDATES TRIPLES to check for their consistency (with graph+Ont.)
            // format: array of strings
            List<String> listCand = new ArrayList<String>();
            listCand = Files.readAllLines(Paths.get(folder_path + file_to_check));
            System.out.println("The number of candidate triples checking is: " + listCand.size());
            List<String> already_added_bc_consistent=new ArrayList<>();
            for(String s:listCand){
                if(axiom_added_strings.contains(s)){
                    System.out.println(s);
                    already_added_bc_consistent.add(s);
                }
            }
            printConsistentTriples(already_added_bc_consistent.toArray(new String[0]), writer_consistent);
            listCand.removeAll(already_added_bc_consistent);
            String[] candidates = listCand.toArray(new String[0]);
            int length_candidates = candidates.length;

            System.out.println("the unsatisfiable classes are:" + reasoner.getUnsatisfiableClasses());



            // 8. DO A BINARY SEARCH ON THE SET OF CANDIDATE TRIPLES TO LOOK FOR CONSISTENT SUBGROUPS
            // Each consistent triple = Array of strings (h,r,t)  so we're putting them all in the hashset because we dont care about the order
            // Collection of all triples= hashset (arrays of strings)
            HashSet<String[]> set_consistent = new HashSet<String[]>();
            HashSet<String[]> set_inconsistent = new HashSet<String[]>();
            BinarySearchForConsistentSubGroups(candidates, ontology, reasoner, manager, dataFactory, length_candidates, writer_consistent);// writer_inconsistent);


            // print the amount of consistent and not consistent
            System.out.println("Number of consistent triples: " + count_cons);
            System.out.println("Number of inconsistent triples: " + count_incons);
            writer_consistent.close();
        }
    }




    //-------------principal functions called directly by the main ---------------------------------------------------------
/*
RECURSIVE FUNCTION that performs the binary search for the consistent subgroups, checking a set of triples, and if inconsistent splitting it into 2. If consistent the triples are left in "ontology",
even if we're not chagning the ontology because it's just a copy that we're using. This is done to keep the triples already found consistent into account in future checks.
 */
    public static void BinarySearchForConsistentSubGroups(String[] linesTocheck, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory, int length_of_candidates, BufferedWriter writer_consistent) throws Exception {   //,BufferedWriter writer_inconsistent )

        // Check if the current linesTocheck are consistent
        if (doTestOn(linesTocheck, ontology, reasoner, manager, dataFactory)) {
            System.out.println("Found consistent group of size:"+length_of_candidates);
            printConsistentTriples(linesTocheck,writer_consistent);
        } else {
            //If the current lines (more than 1) are not consistent, check the 2 halves: right/left of the lines
            if (length_of_candidates > 1) {
                int middle = length_of_candidates / 2;

                //divide set in 2 halves
                String[] leftHalf = Arrays.copyOfRange(linesTocheck, 0, middle);
                String[] rightHalf = Arrays.copyOfRange(linesTocheck, middle, length_of_candidates);

                //recursive call on the 2 halves
                BinarySearchForConsistentSubGroups(leftHalf, ontology, reasoner, manager, dataFactory,  leftHalf.length,writer_consistent);//,writer_inconsistent);
                BinarySearchForConsistentSubGroups(rightHalf, ontology, reasoner, manager, dataFactory,  rightHalf.length,writer_consistent);//,writer_inconsistent);
            }
            else {
                //if We remained with 1 inconsistent triple, that must be inconsistent because of the previous if, return false
                printInconsistentTriples(linesTocheck);//writer_inconsistent);
            }
        }

    }
    /*
    FUNCTION that checks the consistency of a set of triples, 'lines', exploiting the reasoner Hermit.
     */
    public static boolean doTestOn(String[] lines, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception{
        /*add the lines to test to the axioms of the ontology*/
        ontology = addLinestoOntology(lines, ontology, manager, dataFactory);

        try {
            reasoner.flush();

            //check if triples + ontology (containing previous triples detected as 'positive')
            boolean res = reasoner.isConsistent();

            //IF THEY ARE CONSISTENT, KEEP THEM IN THE ONTOLOGY BECAUSE THE following triples have to be consistent also with them
            // IN THIS WAY WE MAKE SURE NOT TO ADD INCONSISTENCIES
            if (!res) ontology = removeLinesFromOntology(lines, ontology, manager, dataFactory);
            else System.out.println("ok");

            reasoner.flush();
            return res;
        }
        catch (IllegalArgumentException e){         //should not happen, if it happens remove lines from ontology and treat thema as inconsistent
            System.out.println(lines[0]);
            ontology=removeLinesFromOntology(lines, ontology, manager, dataFactory);
            //reasoner.flush();
            System.out.println(e);
            return false;
        }
    }
    /*
    FUNCTION that adds some triples in the ontology as assertive axioms to check their consistency
     */
    public static OWLOntology addLinestoOntology(String[] triples_to_add, OWLOntology ontology, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {

        OWLAxiom axiom;
        //for every triple to add
        for (String i : triples_to_add){
            String[] splited_graph = i.split("\\s+");

            //transform the triple  into an assertive axiom to insert it from the ontology
            if (i.contains(typeOf)) {
                axiom = createAxiomTypeOf(splited_graph[0],  removeLastChar(splited_graph[2]), dataFactory);
            }else {
                axiom = createAxiomNotTypeOf(splited_graph[0], splited_graph[1],  removeLastChar(splited_graph[2]), dataFactory,ontology);
            }

            if(axiom!=null) {
                manager.addAxiom(ontology, axiom);
            }
        }

        return ontology;
    }
    /*
    FUNCTION that removes some triples that were inserted in the ontology as assertive axioms to check their consistency
     */
    public static OWLOntology removeLinesFromOntology(String[] triples_to_rem, OWLOntology ontology, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {

        OWLAxiom axiom;

        //for every triple in triples_to_rem
        for (String i : triples_to_rem){
            String[] splited_graph = i.split("\\s+");

            //transform the triple  into an assertive axiom to remove it from the ontology
            if (i.contains(typeOf)) {
                axiom = createAxiomTypeOf(splited_graph[0],  removeLastChar(splited_graph[2]), dataFactory);
            }else {
                axiom = createAxiomNotTypeOf(splited_graph[0], splited_graph[1],  removeLastChar(splited_graph[2]), dataFactory,ontology);
            }

            //remove it
            if(axiom!=null) {
                OWLOntologyChange chg = new RemoveAxiom(ontology, axiom);
                manager.applyChange(chg);
            }

        }

        return ontology;
    }
    /*
    FUNCTION that scans all the triples of the graph and check if they contain some evidence of the membership of some entity to some class
     */
    private static Map<OWLNamedIndividual,OWLClass> findMostProbableClasses(String[] candidates, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {

        Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses = new HashMap<>();
        //scan all the triples of the graph
        for(String s:candidates){
            String[] array_w_s=new String[1];
            array_w_s[0]=s;
            //add them to the ontology and exploit Hermit to determine if they contain some evidence or not
            addLinestoOntology(array_w_s,ontology,manager,dataFactory);
            //check if they contain evidence
            findNewClassesForHeadAndTail(s,allEvidencesOfClasses,ontology,reasoner,dataFactory);
            //remove the triple from the ontology because we have already extracted all the evidence from it
            removeLinesFromOntology(array_w_s,ontology,manager,dataFactory);
        }
        //we want to extract the class with maximum evidence
        Map<OWLNamedIndividual,OWLClass> mostProbableClassesMap=createMapWithClassesWithMaximumEvidence(allEvidencesOfClasses);
        return mostProbableClassesMap;
    }
    /*
    FUNCTION to establish if a triple in s contain some evidence of membership of some entity to some class
     */
    private static void findNewClassesForHeadAndTail(String s, Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses, OWLOntology ontology, Reasoner reasoner, OWLDataFactory dataFactory) {
        //isolate head relation and tail of the triple to inspect looking for evidence
        String[] splited_s = s.split("\\s+");
        splited_s[0] = removeFirstAndLastChar(splited_s[0]);
        splited_s[1] = removeFirstAndLastChar(splited_s[1]);
        splited_s[2] = removeFirstAndLastChar(splited_s[2]);

        OWLNamedIndividual head = dataFactory.getOWLNamedIndividual(IRI.create(splited_s[0]));
        OWLObjectProperty relation = dataFactory.getOWLObjectProperty(IRI.create(splited_s[1]));
        OWLNamedIndividual tail = dataFactory.getOWLNamedIndividual(IRI.create(removeLastChar(splited_s[2])));


        // IF TYPEOF, find classes and superclasses of head and tail
        Set<OWLClassExpression> classes_head = findClassesAndSuperClassesOfEntity(head,reasoner,ontology);
        Set<OWLClassExpression> classes_tail = findClassesAndSuperClassesOfEntity(tail,reasoner,ontology);

        //IF NOT TYPEOF, get domain and range of current relationship
        findDomainOfCurrentRelation(relation,classes_head,reasoner);
        findRangeOfCurrentRelation(relation,classes_tail,reasoner);

        //insert them in the map of all the entities with all the evidence of their classes
        insertInAllEvidenceOfClasses(classes_head, (OWLNamedIndividual) head,allEvidencesOfClasses,reasoner);
        insertInAllEvidenceOfClasses(classes_tail, (OWLNamedIndividual) tail,allEvidencesOfClasses,reasoner);
    }
    /*
    FUNCTION to insert in a map all the evidence: for each entity there are counters that count how much evidence there is of that entity belonging to that class
     */
    private static void insertInAllEvidenceOfClasses(Set<OWLClassExpression> classes_found, OWLNamedIndividual ent, Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses, Reasoner reasoner){
        //for each class for which there is evidence (talking about entity ent)
        for(OWLClassExpression e:classes_found){
            //if there is 0 evidence for that entity for now ->create counters and add the new class found
            if(!allEvidencesOfClasses.containsKey(ent)){
                if(reasoner.isSatisfiable(e)) {
                    Map<OWLClass, Integer> new_class_found = new HashMap<>();
                    new_class_found.put((OWLClass) e.asOWLClass(), 1);
                    allEvidencesOfClasses.put(ent, new_class_found);
                }
            }
            else{              //if there is already evidence for the entity -- increase respective counter

                if(!allEvidencesOfClasses.get(ent).containsKey(e.asOWLClass())){
                    allEvidencesOfClasses.get(ent).put((OWLClass) e.asOWLClass(),1);
                }
                else{
                    int currentCount = allEvidencesOfClasses.get(ent).get(e.asOWLClass());
                    allEvidencesOfClasses.get(ent).put((OWLClass) e.asOWLClass(), currentCount + 1);
                }
            }
        }
    }
    /*
    FUNCTION to insert the most probable class for each entity as a triple in the ontology so that can be used as a guide to decide what to delete and what to keep during the binary search
     */
    private static List<String> InsertProbableClassesInOntology(Map<OWLNamedIndividual, OWLClass> mapMostProbableClasses, OWLOntology ontology, OWLDataFactory dataFactory, OWLOntologyManager manager, Reasoner reasoner) throws Exception {
        String[] stringsToAddToOntology=new String[mapMostProbableClasses.size()];
        List<String> axioms_added_strings=new ArrayList<>();
        int index=0;

        //compose the assertive axiom to add
        for(Map.Entry<OWLNamedIndividual,OWLClass> entry:mapMostProbableClasses.entrySet()){
            stringsToAddToOntology[index]=entry.getKey().toString()+" "+typeOf+" "+entry.getValue().toString()+".";
            axioms_added_strings.add(stringsToAddToOntology[index]);
            index++;
        }
        //add axioms
        addLinestoOntology(stringsToAddToOntology,ontology,manager,dataFactory);
        return axioms_added_strings;
    }


///------------------utils not called directly by the main --------------------------------------------------------------------

    /*
    FUNCTION that print the number of inconsistent triples, just to keep track of them during the binary search
    */
    private static void printInconsistentTriples(String[] setInconsistent){  //, BufferedWriter writer_inconsistent ) throws IOException {
        for (String j : setInconsistent){
            count_incons++;
            System.out.println("Inconsistent: "+j);
        }

        System.out.println("le inconsistent sono:"+count_incons);
    }
    /*
    FUNCTION that print the consistent triples that are the triples we're interestedd in and that will be used in the next steps
    */
    private static void printConsistentTriples(String[] setConsistent, BufferedWriter writer_consistent ) throws IOException {
        for (String j : setConsistent){
            count_cons++;
            writer_consistent.write(j);
            writer_consistent.newLine();
        }

        System.out.println("le consistent sono:"+count_cons);
    }
    /*
    FUNCTION UTIL remove last char from string s
    */
    public static String removeLastChar(String s) {
        return s.substring(0, s.length() - 1);
    }
    /*
     FUNCTION UTIL remove first and last chars from string s
    */
    private static String removeFirstAndLastChar(String s) {
        return s.substring(1,s.length()-1);
    }
    /*
     FUNCTION UTIL create an assertive axiom given (h,r,t), used to express a triple in the form of an assertive axiom
    */
    public static OWLAxiom createAxiomNotTypeOf(String h, String r, String t, OWLDataFactory factory, OWLOntology ontology) throws Exception{
        //format the string to create the axiom
        h = h.replace("<", "");
        h = h.replace(">", "");
        r = r.replace("<", "");
        r = r.replace(">", "");
        t = t.replace("<", "");
        t = t.replace(">", "");
        OWLIndividual head = factory.getOWLNamedIndividual(IRI.create(h));
        OWLIndividual tail = factory.getOWLNamedIndividual(IRI.create(t));
        OWLObjectProperty relation = factory.getOWLObjectProperty(IRI.create(r));

        OWLObjectPropertyAssertionAxiom axiom = factory.getOWLObjectPropertyAssertionAxiom(relation, head, tail);

        OWLProperty property= (OWLProperty) axiom.getProperty();

        //if property in the triple is not recognized as a property in the ontolgyh can not create axiom
        if(!ontology.containsObjectPropertyInSignature((property.getIRI()))) {
            return null;
        }

        return axiom;
    }
    /*
     FUNCTION UTIL create an assertive axiom given (h,r,t), used to express a triple in the form of an assertive axiom
    */
    public static OWLAxiom createAxiomTypeOf(String h, String t, OWLDataFactory factory) throws Exception{
        //format the string to create the axiom

        h = h.replace("<", "");
        h = h.replace(">", "");
        t = t.replace("<", "");
        t = t.replace(">", "");

        OWLIndividual head = factory.getOWLNamedIndividual(IRI.create(h));
        OWLClass tail = factory.getOWLClass(IRI.create(t));
        OWLAxiom axiom  = factory.getOWLClassAssertionAxiom(tail, head);

        return axiom;
    }
    /*
    FUNCTION to collect the evidence in a triple 'typeof' x-type-y... we look for equivalent classes of y and superclasses of y
     */
    private static Set<OWLClassExpression>  findClassesAndSuperClassesOfEntity(OWLNamedIndividual entity, Reasoner reasoner, OWLOntology ontology) {
        reasoner.getPrecomputableInferenceTypes();
        //get the types of the entity with the reasoner (that extracts only the class of the tail)
        Set<OWLClassExpression> classes_head = entity.getTypes(ontology);

        //for each class found (that will be only the tail) add the equivalent classes and superclasses, both returned by reasoner.getSuperClasses
        for(OWLClassExpression e:classes_head) {
            if(!Objects.equals(e.toString(),"owl:Thing"))classes_head.add(e);
            NodeSet<OWLClass> nodeset= reasoner.getSuperClasses(e.asOWLClass(),true);
            for(OWLClass c:nodeset.getFlattened()){
                if(!Objects.equals(c.toString(),"owl:Thing")){
                    classes_head.add(c);
                }
            }
        }
        return classes_head;
    }
    /*
    FUNCTION to collect the evidence in a triple 'noTypeOf' x-r-y ... we look for equivalent classes and superclasses of the classes in the domain of r
    and those will be classes for which the entity x has an evidence of membership
     */
    private static void findDomainOfCurrentRelation(OWLObjectProperty relation, Set<OWLClassExpression> classesHead, Reasoner reasoner) {
        //also equivalent classes are returned here
        NodeSet<OWLClass> nodeset=reasoner.getObjectPropertyDomains(relation,true);

        //here i add also superclasses of the classes in the domain
        for(OWLClass c:nodeset.getFlattened()) {
            if(!Objects.equals(c.toString(),"owl:Thing")) {
                classesHead.add(c);
                NodeSet<OWLClass> nodeSet_super=reasoner.getSuperClasses(c,true);
                for (OWLClass sc : nodeSet_super.getFlattened()) {
                    if (!Objects.equals(sc.toString(), "owl:Thing")) {
                        classesHead.add(sc);
                    }
                }
            }
        }
    }
    /*
    FUNCTION to collect the evidence in a triple 'noTypeOf' x-r-y ... we look for equivalent classes and superclasses of the classes in the range of r
    and those will be classes for which the entity x has an evidence of membership
     */
    private static void findRangeOfCurrentRelation(OWLObjectProperty relation, Set<OWLClassExpression> classesTail, Reasoner reasoner) {
        //also equivalent classes are returned here
        NodeSet<OWLClass> nodeset=reasoner.getObjectPropertyRanges(relation,true);

        //here i add also superclasses of the classes in the range
        for(OWLClass c:nodeset.getFlattened()) {
            if(!Objects.equals(c.toString(),"owl:Thing")) {
                classesTail.add(c);
                NodeSet<OWLClass> nodeSet_super=reasoner.getSuperClasses(c,true);
                for (OWLClass sc : nodeSet_super.getFlattened()) {
                    if (!Objects.equals(sc.toString(), "owl:Thing")) {
                        classesTail.add(sc);
                    }
                }
            }
        }
    }

    /*
    FUNCTION to select for each entity the class with the highest counter
     */
    private static Map<OWLNamedIndividual, OWLClass> createMapWithClassesWithMaximumEvidence(Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses) {
        Map<OWLNamedIndividual, OWLClass> classWithMaxValueMap = new HashMap<>();

        //for each entity for which we collected evidence
        for (Map.Entry<OWLNamedIndividual, Map<OWLClass, Integer>> entry : allEvidencesOfClasses.entrySet()) {
            OWLNamedIndividual individual = entry.getKey();
            Map<OWLClass, Integer> classMap = entry.getValue();

            int maxValue = Integer.MIN_VALUE;
            OWLClass classWithMaxValue = null;

            //iterate to find highest counter
            for (Map.Entry<OWLClass, Integer> classEntry : classMap.entrySet()) {
                int value = classEntry.getValue();

                if (value > maxValue) {
                    maxValue = value;
                    classWithMaxValue = classEntry.getKey();
                }
            }

            classWithMaxValueMap.put(individual, classWithMaxValue);
        }

        return classWithMaxValueMap;
    }
}