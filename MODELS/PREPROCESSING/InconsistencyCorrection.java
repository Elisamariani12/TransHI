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
    public static String folder_path;
    public static Integer number_set=1;
    public static Integer count_cons=0;
    public static Integer count_incons=0;


    public static void main(String[] args) throws Exception {
        folder_path=args[0];
        String name_ontology=args[1];

        for(int i=1;i<4;i++) {
            number_set=i;
            String file_to_create;
            String file_to_check;

            //0. NAME FILE TO CREATE AND TO CHECK
            if (number_set == 1){ file_to_create = "consistent_triples_train_TRY.ttl"; file_to_check="train.ttl";}
            else if (number_set == 2){ file_to_create = "consistent_triples_test_TRY.ttl";file_to_check="test.ttl";}
            else { file_to_create = "consistent_triples_valid_TRY.ttl";file_to_check="valid.ttl";}

            // 1. CREATE OWLOntologyManager object TO LOAD & SAVE ONTOLOGIES
            OWLOntologyManager manager = OWLManager.createOWLOntologyManager();

            // 2. CREATE DATA FACTORY to create axioms and WRITER to write in the file
            OWLDataFactory dataFactory = manager.getOWLDataFactory();
            BufferedWriter writer_consistent = new BufferedWriter(new FileWriter(folder_path + file_to_create));

            // 3. LOAD THE ONTOLOGY using the OWL API
            OWLOntology Real_ontology = manager.loadOntologyFromOntologyDocument(new File(folder_path + name_ontology ));
            // Create a temporary copy of the ontology to not change the file
            OWLOntology ontology = manager.createOntology();
            for (OWLAxiom axiom : Real_ontology.getAxioms()) {manager.addAxiom(ontology, axiom);}
            System.out.println("ontology loaded. Axioms counted: " + ontology.getAxiomCount());

            // 4. CREATE A REASONER
            Reasoner reasoner = new Reasoner(ontology);

            // 5 FIND FOR EACH ENTITY THE CLASS OF APPARTENENCE FOR WHICH WE HAVE MORE EVIDENCE
            String[] completeGraph = Files.readAllLines(Paths.get(folder_path + "complete_graph.ttl")).toArray(new String[0]);
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

            // 8. LOAD THE CANDIDATES TRIPLES to check for their consistency (with graph+Ont.)
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



            // 10 DO A BINARY SEARCH ON THE SET OF CANDIDATE TRIPLES TO LOOK FOR CONSISTENT SUBGROUPS
            // Each consistent triple = Array of strings (h,r,t)  so we're putting them all in the hashset because we dont care about the order
            // Collection of all triples= hashset (arrays of strings)
            HashSet<String[]> set_consistent = new HashSet<String[]>();
            HashSet<String[]> set_inconsistent = new HashSet<String[]>();
            BinarySearchForConsistentSubGroups(candidates, ontology, reasoner, manager, dataFactory, length_candidates, writer_consistent);// writer_inconsistent);


            //for me... print the amount of consistent and not consistent
            System.out.println("Number of consistent triples: " + count_cons);
            System.out.println("Number of inconsistent triples: " + count_incons);
            writer_consistent.close();
            //writer_inconsistent.close();
        }
    }




//-------------principal functions called directly by the main ---------------------------------------------------------

public static void BinarySearchForConsistentSubGroups(String[] linesTocheck, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory, int length_of_candidates, BufferedWriter writer_consistent) throws Exception {   //,BufferedWriter writer_inconsistent )
        //System.out.println("BINARY SEARCH call- size:"+length_of_candidates);
        //for(String s:linesTocheck)System.out.println(s+",");
        // Check if the current linesTocheck are consistent
        if (doTestOn(linesTocheck, ontology, reasoner, manager, dataFactory)) {
        System.out.println("Found consistent group of size:"+length_of_candidates);
        printConsistentTriples(linesTocheck,writer_consistent);
        } else {
        //If the current lines (more than 1) are not consistent, check the 2 halves: right/left of the lines
        if (length_of_candidates > 1) {
        int middle = length_of_candidates / 2;

        String[] leftHalf = Arrays.copyOfRange(linesTocheck, 0, middle);
        String[] rightHalf = Arrays.copyOfRange(linesTocheck, middle, length_of_candidates);

        BinarySearchForConsistentSubGroups(leftHalf, ontology, reasoner, manager, dataFactory,  leftHalf.length,writer_consistent);//,writer_inconsistent);
        BinarySearchForConsistentSubGroups(rightHalf, ontology, reasoner, manager, dataFactory,  rightHalf.length,writer_consistent);//,writer_inconsistent);
        }
        else {
        //if We remained with 1 inconsistent triple, that must be inconsistent because of the previous if, return false
            printInconsistentTriples(linesTocheck);//writer_inconsistent);
        }
        }

        }

public static boolean doTestOn(String[] lines, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception{
        /*This function test the lines and check whether they're consistent with the ontology(to which the graph has already been added)*/
        System.out.println("size pre:"+ontology.getAxiomCount());
        ontology = addLinestoOntology(lines, ontology, manager, dataFactory);
        System.out.println("size post:"+ontology.getAxiomCount());

        //System.out.println(ontology.getAxiomCount());
        try {
        reasoner.flush();


        //System.out.println("reasoner created");
        boolean res = reasoner.isConsistent();

        //IF THEY ARE CONSISTENT, KEEP THEM IN THE ONTOLOGY BECAUSE THEY MIGHT BE INCONSISTENT WITH OTHER TRIPLES IN THE SET THAT
        // WE'RE CHECKING.. IN THIS WAY WE MAKE SURE NOT TO ADD INCONSISTENCIES
        if (!res) ontology = removeLinesFromOntology(lines, ontology, manager, dataFactory);
        else System.out.println("ok");
        reasoner.flush();
        //System.out.println("axioms deleted");
        //System.out.println(ontology3.getAxiomCount());

        return res;
        }
        catch (IllegalArgumentException e){
        System.out.println(lines[0]);
        ontology=removeLinesFromOntology(lines, ontology, manager, dataFactory);
        //reasoner.flush();
        System.out.println(e);
        return false;
        }
        }

public static OWLOntology addLinestoOntology(String[] graph, OWLOntology ontology, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {
        //List<String> listGraph = new ArrayList<String>();
        //listGraph = Files.readAllLines(Paths.get("triples_DBYAGO2.ttl"));
        //String[] graph = listGraph.toArray(new String[0]);
        OWLAxiom axiom;
        for (String i : graph){
        String[] splited_graph = i.split("\\s+"); //regex space & attention point à la fin
        if (i.contains(typeOf)) {
            axiom = createAxiomTypeOf(splited_graph[0],  removeLastChar(splited_graph[2]), dataFactory);
            //if(splited_graph[0].equals("<http://dbpedia.org/resource/Auburn_University>")){System.out.println(axiom);}

        }else {
            axiom = createAxiomNotTypeOf(splited_graph[0], splited_graph[1],  removeLastChar(splited_graph[2]), dataFactory,ontology);
            //if(splited_graph[0].equals("<http://dbpedia.org/resource/Auburn_University>")){System.out.println(axiom);}
        }
        //System.out.print("Axioms are:"+ ontology.getAxiomCount());
        if(axiom!=null) {
        //System.out.println("added axiom:"+axiom);
        manager.addAxiom(ontology, axiom);
        }
        //manager.saveOntology(ontology);
        }
        //manager.saveOntology(ontology);
        //System.out.println("axioms added");
        //manager.saveOntology(ontology);
        return ontology;
        }

public static OWLOntology removeLinesFromOntology(String[] graph, OWLOntology ontology, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {
        //List<String> listGraph = new ArrayList<String>();
        //listGraph = Files.readAllLines(Paths.get("triples_DBYAGO2.ttl"));
        //String[] graph = listGraph.toArray(new String[0]);
        OWLAxiom axiom;
        for (String i : graph){
            //System.out.println("Rimuovo:"+i);
            String[] splited_graph = i.split("\\s+"); //regex space & attention point à la fin

            if (i.contains(typeOf)) {
            axiom = createAxiomTypeOf(splited_graph[0],  removeLastChar(splited_graph[2]), dataFactory);

            }else {
            axiom = createAxiomNotTypeOf(splited_graph[0], splited_graph[1],  removeLastChar(splited_graph[2]), dataFactory,ontology);

            }
            //System.out.println("Remove axiom: " + axiom.toString());
            if(axiom!=null) {
            OWLOntologyChange chg = new RemoveAxiom(ontology, axiom);
            manager.applyChange(chg);
            }
            //manager.removeAxiom(ontology, axiom);
            //manager.saveOntology(ontology);
        }
        //manager.saveOntology(ontology);
        //System.out.println("axioms removed");
        //manager.saveOntology(ontology);
        return ontology;
        }

private static Map<OWLNamedIndividual,OWLClass> findMostProbableClasses(String[] candidates, OWLOntology ontology, Reasoner reasoner, OWLOntologyManager manager, OWLDataFactory dataFactory) throws Exception {

        Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses = new HashMap<>();
        for(String s:candidates){
        String[] array_w_s=new String[1];
        array_w_s[0]=s;
        //System.out.println(s);
        addLinestoOntology(array_w_s,ontology,manager,dataFactory);
        findNewClassesForHeadAndTail(s,allEvidencesOfClasses,ontology,reasoner,dataFactory);
        removeLinesFromOntology(array_w_s,ontology,manager,dataFactory);
        }
        Map<OWLNamedIndividual,OWLClass> mostProbableClassesMap=createMapWithClassesWithMaximumEvidence(allEvidencesOfClasses);

        //System.out.println(mostProbableClassesMap);

        return mostProbableClassesMap;
        }

private static void findNewClassesForHeadAndTail(String s, Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses, OWLOntology ontology, Reasoner reasoner, OWLDataFactory dataFactory) {
        String[] splited_s = s.split("\\s+");
        splited_s[0] = removeFirstAndLastChar(splited_s[0]);
        splited_s[1] = removeFirstAndLastChar(splited_s[1]);
        splited_s[2] = removeFirstAndLastChar(splited_s[2]);

        OWLNamedIndividual head = dataFactory.getOWLNamedIndividual(IRI.create(splited_s[0]));
        OWLObjectProperty relation = dataFactory.getOWLObjectProperty(IRI.create(splited_s[1]));
        OWLNamedIndividual tail = dataFactory.getOWLNamedIndividual(IRI.create(removeLastChar(splited_s[2])));

        //reasoner.getPrecomputableInferenceTypes();

        // IF TYPEOF, find classes and superclasses of head and tail
        Set<OWLClassExpression> classes_head = findClassesAndSuperClassesOfEntity(head,reasoner,ontology);
        Set<OWLClassExpression> classes_tail = findClassesAndSuperClassesOfEntity(tail,reasoner,ontology);

        //IF NOT TYPEOF, get domain and range of current relationship
        findDomainOfCurrentRelation(relation,classes_head,reasoner);
        findRangeOfCurrentRelation(relation,classes_tail,reasoner);

        //to check the results
        //System.out.println(classes_head.toString());
        //System.out.println(classes_tail.toString());

        //insert them in the map of all the entities with all the evidence of their classes
        insertInAllEvidenceOfClasses(classes_head, (OWLNamedIndividual) head,allEvidencesOfClasses,reasoner);
        insertInAllEvidenceOfClasses(classes_tail, (OWLNamedIndividual) tail,allEvidencesOfClasses,reasoner);
        }

private static void insertInAllEvidenceOfClasses(Set<OWLClassExpression> classes_found, OWLNamedIndividual head, Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses, Reasoner reasoner){
        for(OWLClassExpression e:classes_found){

        if(!allEvidencesOfClasses.containsKey(head)){
        if(reasoner.isSatisfiable(e)) {
        Map<OWLClass, Integer> new_class_found = new HashMap<>();
        new_class_found.put((OWLClass) e.asOWLClass(), 1);
        allEvidencesOfClasses.put(head, new_class_found);
        }
        }
        else{
        if(!allEvidencesOfClasses.get(head).containsKey(e.asOWLClass())){
        allEvidencesOfClasses.get(head).put((OWLClass) e.asOWLClass(),1);
        }
        else{
        int currentCount = allEvidencesOfClasses.get(head).get(e.asOWLClass());
        allEvidencesOfClasses.get(head).put((OWLClass) e.asOWLClass(), currentCount + 1);
        }
        }
        }
        }

private static List<String> InsertProbableClassesInOntology(Map<OWLNamedIndividual, OWLClass> mapMostProbableClasses, OWLOntology ontology, OWLDataFactory dataFactory, OWLOntologyManager manager, Reasoner reasoner) throws Exception {
        String[] stringsToAddToOntology=new String[mapMostProbableClasses.size()];
        List<String> axioms_added_strings=new ArrayList<>();
        int index=0;
        for(Map.Entry<OWLNamedIndividual,OWLClass> entry:mapMostProbableClasses.entrySet()){
        stringsToAddToOntology[index]=entry.getKey().toString()+" "+typeOf+" "+entry.getValue().toString()+".";
        axioms_added_strings.add(stringsToAddToOntology[index]);
        //System.out.println(stringsToAddToOntology[index]);
        index++;
        }

        addLinestoOntology(stringsToAddToOntology,ontology,manager,dataFactory);
    return axioms_added_strings;
}


///------------------utils not called directly by the main --------------------------------------------------------------------

private static void printInconsistentTriples(String[] setInconsistent){  //, BufferedWriter writer_inconsistent ) throws IOException {
        for (String j : setInconsistent){
            count_incons++;
            System.out.println("Inconsistent: "+j);
        //    writer_inconsistent.write(j);
        //    writer_inconsistent.newLine();
        }

        System.out.println("le inconsistent sono:"+count_incons);
        }

private static void printConsistentTriples(String[] setConsistent, BufferedWriter writer_consistent ) throws IOException {
        for (String j : setConsistent){
        count_cons++;
        writer_consistent.write(j);
        writer_consistent.newLine();
        }

        System.out.println("le consistent sono:"+count_cons);
        }

public static String removeLastChar(String s) {
        return s.substring(0, s.length() - 1);
        }

private static String removeFirstAndLastChar(String s) {
        return s.substring(1,s.length()-1);
        }

public static OWLAxiom createAxiomNotTypeOf(String h, String r, String t, OWLDataFactory factory, OWLOntology ontology) throws Exception{
        h = h.replace("<", "");
        h = h.replace(">", "");
        r = r.replace("<", "");
        r = r.replace(">", "");
        t = t.replace("<", "");
        t = t.replace(">", "");
        //System.out.println("Created axiomNotTypeOf");
        OWLIndividual head = factory.getOWLNamedIndividual(IRI.create(h));
        OWLIndividual tail = factory.getOWLNamedIndividual(IRI.create(t));
        OWLObjectProperty relation = factory.getOWLObjectProperty(IRI.create(r));

        OWLObjectPropertyAssertionAxiom axiom = factory.getOWLObjectPropertyAssertionAxiom(relation, head, tail);

        OWLProperty property= (OWLProperty) axiom.getProperty();
        if(!ontology.containsObjectPropertyInSignature((property.getIRI()))) {
            //System.out.println("property non nell ont:"+property.toString());
            return null;
        }

        return axiom;
        }

public static OWLAxiom createAxiomTypeOf(String h, String t, OWLDataFactory factory) throws Exception{
        //System.out.println("Created axiomTypeOf");

        h = h.replace("<", "");
        h = h.replace(">", "");
        t = t.replace("<", "");
        t = t.replace(">", "");

        OWLIndividual head = factory.getOWLNamedIndividual(IRI.create(h));
        //OWLClassExpression tail = factory.getOWLClass(IRI.create(t));
        OWLClass tail = factory.getOWLClass(IRI.create(t));
        OWLAxiom axiom  = factory.getOWLClassAssertionAxiom(tail, head);

        //OWLAxiom axiom = factory.getOWLClassAssertionAxiom(tail, head);
        return axiom;
        }

private static Set<OWLClassExpression>  findClassesAndSuperClassesOfEntity(OWLNamedIndividual entity, Reasoner reasoner, OWLOntology ontology) {
        reasoner.getPrecomputableInferenceTypes();
        Set<OWLClassExpression> classes_head = entity.getTypes(ontology);
        for(OWLClassExpression e:classes_head) {
        if(!Objects.equals(e.toString(),"owl:Thing"))classes_head.add(e);
        NodeSet<OWLClass> nodeset= reasoner.getSuperClasses(e.asOWLClass(),true);
        for(OWLClass c:nodeset.getFlattened()){
        if(!Objects.equals(c.toString(),"owl:Thing")){
        classes_head.add(c);}
        }
        }
        return classes_head;
        }

private static void findDomainOfCurrentRelation(OWLObjectProperty relation, Set<OWLClassExpression> classesHead, Reasoner reasoner) {
        //also equivalent classes are returned here
        NodeSet<OWLClass> nodeset=reasoner.getObjectPropertyDomains(relation,true);
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

private static void findRangeOfCurrentRelation(OWLObjectProperty relation, Set<OWLClassExpression> classesTail, Reasoner reasoner) {
        NodeSet<OWLClass> nodeset=reasoner.getObjectPropertyRanges(relation,true);
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

    private static Map<OWLNamedIndividual, OWLClass> createMapWithClassesWithMaximumEvidence(Map<OWLNamedIndividual, Map<OWLClass, Integer>> allEvidencesOfClasses) {
        Map<OWLNamedIndividual, OWLClass> classWithMaxValueMap = new HashMap<>();

        for (Map.Entry<OWLNamedIndividual, Map<OWLClass, Integer>> entry : allEvidencesOfClasses.entrySet()) {
            OWLNamedIndividual individual = entry.getKey();
            Map<OWLClass, Integer> classMap = entry.getValue();

            int maxValue = Integer.MIN_VALUE;
            OWLClass classWithMaxValue = null;

            for (Map.Entry<OWLClass, Integer> classEntry : classMap.entrySet()) {
                int value = classEntry.getValue();

                if (value > maxValue) {
                    maxValue = value;
                    classWithMaxValue = classEntry.getKey();
                    //if(individual.toString().equals("<http://dbpedia.org/resource/Auburn_University>"))
                    //    System.out.println(individual.toString()+"--"+value+"--"+classEntry.getKey());
                }
            }

            classWithMaxValueMap.put(individual, classWithMaxValue);
        }

        return classWithMaxValueMap;
    }
}