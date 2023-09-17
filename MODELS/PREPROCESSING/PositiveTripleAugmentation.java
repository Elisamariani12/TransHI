import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.util.stream.Collectors;

public class PositiveTripleAugmentation {
    public static Integer IDTypeOf;
    public static final String typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";

    public static String folder_path; //folder where there are the axioms extracted fromt the ontology

    public static void main(String[] args) throws Exception {
        folder_path=args[0];

        //Load the files containing the axioms information
        Map<Integer, Set<Integer>> dictionary_of_equivalentClasses=fillDictionary(folder_path+"EquivalentClasses_axioms.txt");
        Map<Integer, Set<Integer>> dictionary_of_superclasses= fillDictionary(folder_path+"SuperClasses_axioms.txt");
        Map<Integer, Set<Integer>> dictionary_of_superProperties= fillDictionary(folder_path+"SuperProperties_axioms.txt");
        Map<Integer, Set<Integer>> dictionary_of_equivalentProperties= fillDictionary(folder_path+"EquivalentProperties_axioms.txt");
        Map<Integer, Set<Integer>> dictionary_of_inverseProperties= fillDictionary(folder_path+"InverseProperties_axioms.txt");


        //Load train triples file
        List<Triple_Integer> listTriplesID=loadTriples(folder_path+"train2id_Consistent.txt");
        System.out.println("The original triples are:"+listTriplesID.size());

        //load relation2id & get the ID of typeOf relationship
        getTypeOf_ID(folder_path+"relation2id.txt");

        //Create a dictionary that maps each entity to all of its classes
        Map<Integer, Set<Integer>> map_entity_to_classes= new HashMap<>();
        fillMap_entity_to_classes(map_entity_to_classes,listTriplesID);

        //-----------------NEW POSITIVES CREATION ---------------------------------------
        Set<Triple_Integer> createdPositives= new HashSet<>();

        //1. USING EQUIVALENT CLASSES --  typeOf new triples
        createWithEquivalentClasses(createdPositives,dictionary_of_equivalentClasses,listTriplesID);
        System.out.println("I created "+createdPositives.size()+" new triples");

        //2. USING SUPERCLASSES -- typeOf new triples
        createWithSuperClasses(createdPositives,dictionary_of_superclasses,listTriplesID);
        System.out.println("I created "+createdPositives.size()+" new triples");

        //3. USING SUPERproperties -- NotTypeOf new triples
        createWithSuperProperties(createdPositives,dictionary_of_superProperties,listTriplesID);
        System.out.println("I created "+createdPositives.size()+" new triples");

        //4. USING EquivalentProperties -- NotTypeOf new triples
        createWithEquivalentProperties(createdPositives,dictionary_of_equivalentProperties,listTriplesID);
        System.out.println("I created "+createdPositives.size()+" new triples");

        //4. USING InverseProperties -- NotTypeOf new triples
        createWithInverseProperties(createdPositives,dictionary_of_inverseProperties,listTriplesID);
        System.out.println("I created "+createdPositives.size()+" new triples");


        listTriplesID.addAll(createdPositives);
        System.out.println("Now the total of positive triple is : "+listTriplesID.size());
        printAdditionalTriplesInFile(folder_path+"train2id_Consistent_withAugmentationV2.txt",listTriplesID);
    }

    /*
    FUNCTION to print the new created triples in a file together with the other training triples
     */
    private static void printAdditionalTriplesInFile(String s, List<Triple_Integer> listTriplesID) throws IOException {
        File file = new File(s);
        System.out.println("I deleted the file because it was already existing:"+file.delete());
        FileWriter writer = new FileWriter(s,true);
        BufferedWriter bw = new BufferedWriter(writer);

        bw.write(String.valueOf(listTriplesID.size()));
        bw.newLine();

        for(Triple_Integer t:listTriplesID){
            bw.write(t.getSubject()+" "+ t.getObject()+" "+t.getPredicate());
            if(listTriplesID.get(listTriplesID.size()-1)!=t)bw.newLine();
        }

        bw.close();
    }

    /*
    FUNCTION to create from triples x-type->y other triples x-type-> EquivalentClasses(y)
     */
    private static void createWithEquivalentClasses(Set<Triple_Integer> createdPositives, Map<Integer, Set<Integer>> dictionaryOfEquivalentClasses, List<Triple_Integer> listTriplesID) {
        int count_perc=0;
        System.out.println("1st creation using"+dictionaryOfEquivalentClasses.keySet().size()+ " axioms of equivalent classes");

        if (dictionaryOfEquivalentClasses.size()==0)return;

        //for each train triples 'typeof' ...
        for(Triple_Integer t:listTriplesID){
            if(Objects.equals(t.getPredicate(), IDTypeOf)){
                // if there are classes equivalent to the tail
                if(dictionaryOfEquivalentClasses.get(t.getObject())!=null){
                    //create new triples using the equiv.classes as new tails
                    for(Integer subsitute:dictionaryOfEquivalentClasses.get(t.getObject())){
                        Triple_Integer new_t=new Triple_Integer(t.getSubject(),t.getPredicate(),subsitute);
                        if(!is_already_present_inSet(createdPositives,new_t)){
                            if(!is_already_present_inList(listTriplesID,new_t)) {
                                createdPositives.add(new_t);
                            }
                        }
                    }
                }
            }
            count_perc++;
        }
    }
    /*
    FUNCTION to create from triples x-type->y other triples x-type-> SuperClasses(y)
    */
    private static void createWithSuperClasses(Set<Triple_Integer> createdPositives, Map<Integer, Set<Integer>> dictionaryOfSuperclasses, List<Triple_Integer> listTriplesID) {
        int count_perc=0;
        System.out.println("2nd creation using"+dictionaryOfSuperclasses.keySet().size()+ " axioms of super classes");

        if(dictionaryOfSuperclasses.size()==0)return;

        //for each train triples 'typeof' ...
        for(Triple_Integer t:listTriplesID){
            if(Objects.equals(t.getPredicate(), IDTypeOf)){
                // if there are superclasses of the tail
                if(dictionaryOfSuperclasses.get(t.getObject())!=null){
                    //create new triples using the superclasses as new tails
                    for(Integer subsitute:dictionaryOfSuperclasses.get(t.getObject())){
                        Triple_Integer new_t=new Triple_Integer(t.getSubject(),t.getPredicate(),subsitute);
                        if(!is_already_present_inSet(createdPositives,new_t)){
                            if(!is_already_present_inList(listTriplesID,new_t)) {
                                createdPositives.add(new_t);
                            }
                        }
                    }
                }
            }
            count_perc++;
        }
    }
    /*
        FUNCTION to create from triples x-p->y other triples x-superprop(p)->y
    */
    private static void createWithSuperProperties(Set<Triple_Integer> createdPositives, Map<Integer, Set<Integer>> dictionaryOfSuperProperties, List<Triple_Integer> listTriplesID) {
        int count_perc=0;
        System.out.println("3rd creation using"+dictionaryOfSuperProperties.keySet().size()+ " axioms of super properties");

        if(dictionaryOfSuperProperties.size()==0)return;

        //for each train triples 'notypeof' ...
        for(Triple_Integer t:listTriplesID){
            if(!Objects.equals(t.getPredicate(), IDTypeOf)){
                // if there are superproperties of the relation
                if(dictionaryOfSuperProperties.get(t.getPredicate())!=null){
                    //create new triples using superproperties as the relation
                        for (Integer subsitute : dictionaryOfSuperProperties.get(t.getPredicate())) {
                            Triple_Integer new_t = new Triple_Integer(t.getSubject(), subsitute, t.getObject());
                            if (!is_already_present_inList(listTriplesID, new_t)) {
                                createdPositives.add(new_t);

                            }
                        }

                }
            }
            count_perc++;
        }
    }
    /*
        FUNCTION to create from triples x-p->y other triples x-equiv.prop(p)->y
    */
    private static void createWithEquivalentProperties(Set<Triple_Integer> createdPositives, Map<Integer, Set<Integer>> dictionaryOfEquivalentProperties, List<Triple_Integer> listTriplesID) {
        int count_perc=0;
        System.out.println("4th creation using"+dictionaryOfEquivalentProperties.keySet().size()+ " axioms of equivalent properties");
        if(dictionaryOfEquivalentProperties.size()==0)return;

        //for each train triples 'notypeof' ...
        for(Triple_Integer t:listTriplesID){
            if(!Objects.equals(t.getPredicate(), IDTypeOf)){
                // if there are superproperties of the relation
                if(dictionaryOfEquivalentProperties.get(t.getPredicate())!=null){
                    //create new triples using superproperties as the relation
                    for (Integer subsitute : dictionaryOfEquivalentProperties.get(t.getPredicate())) {
                        Triple_Integer new_t = new Triple_Integer(t.getSubject(), subsitute, t.getObject());
                        if (!is_already_present_inList(listTriplesID, new_t)) {
                            createdPositives.add(new_t);
                        }
                    }
                }
            }
            count_perc++;
        }
    }
    /*
        FUNCTION to create from triples x-p->y other triples y-inverse.prop(p)->x
    */
    private static void createWithInverseProperties(Set<Triple_Integer> createdPositives, Map<Integer, Set<Integer>> dictionaryOfInverseProperties, List<Triple_Integer> listTriplesID) {
        int count_perc=0;
        System.out.println("5th creation using"+dictionaryOfInverseProperties.keySet().size()+ " axioms of inverse properties");

        if(dictionaryOfInverseProperties.size()==0)return;

        //for each train triples 'notypeof' ...
        for(Triple_Integer t:listTriplesID){
            if(!Objects.equals(t.getPredicate(), IDTypeOf)){
                // if there are inverse prop of the relation
                if(dictionaryOfInverseProperties.get(t.getPredicate())!=null){
                    //create new triples using inverse prop. as the relation and exhanging head and tail
                    for (Integer subsitute : dictionaryOfInverseProperties.get(t.getPredicate())) {
                        Triple_Integer new_t = new Triple_Integer(t.getObject(), subsitute, t.getSubject());
                        if (!is_already_present_inList(listTriplesID, new_t)) {
                            createdPositives.add(new_t);
                        }
                    }
                }
            }
            count_perc++;
        }
    }



    //importing files functions --------------------------------------------------------------------------------------
     /*
      FUNCTION UTIL fills a dictionary with the content of a file of numbers on different lines separated by commas
      */
    private static Map<Integer, Set<Integer>> fillDictionary(String filepath) {
        Map<Integer, Set<Integer>> dictionaryToFill=new HashMap<>();
        try {
             dictionaryToFill = Files.lines(Paths.get(filepath))
                    .map(line -> line.split(","))
                    .collect(Collectors.toMap(
                            parts -> Integer.parseInt(parts[0]),
                            parts -> {
                                Set<Integer> values = new HashSet<>();
                                for (int i = 1; i < parts.length; i++) {
                                    values.add(Integer.parseInt(parts[i]));
                                }
                                return values;
                            }
                    ));
        } catch (IOException e) {
            e.printStackTrace();
        }
        return dictionaryToFill;
    }

    /*
      FUNCTION UTIL loads the triples of the trainset as a list of triple_integer
      */
    private static List<Triple_Integer> loadTriples(String path) throws IOException {
        //loads a triple separating head, tail and relation and creating a triple_integer with them
        List<Triple_Integer> listTriplesID = Files.lines(Paths.get(path))
                .skip(1)
                .map(line -> {
                    String[] parts = line.split(" ");
                    Integer subject = Integer.parseInt(parts[0]);
                    Integer object = Integer.parseInt(parts[1]);
                    Integer predicate = Integer.parseInt(parts[2]);
                    return new Triple_Integer(subject, predicate, object);
                })
                .collect(Collectors.toList());
        return listTriplesID;
    }
    //utils ------------------------------------------------------------------------------------------------------------

    /*
    FUNCTION UTIL fills a mapEntityToClasses with the entities as key and their classes as value
    */
    private static void fillMap_entity_to_classes(Map<Integer, Set<Integer>> mapEntityToClasses, List<Triple_Integer> listTriplesID) {
        //for each triple of the trainset
        for(Triple_Integer t: listTriplesID){
            //if it's a typeof triple
            if(Objects.equals(t.getPredicate(), IDTypeOf)){
                //add the class to the set of classes of the head of the triple
                if(!mapEntityToClasses.containsKey(t.getSubject())){
                    Set<Integer> newSet = new HashSet<>();
                    newSet.add(t.getObject());
                    mapEntityToClasses.put(t.getSubject(), newSet);
                }
                else{
                    mapEntityToClasses.get(t.getSubject()).add(t.getObject());
                }
            }
        }
    }
    /*
    FUNCTION UTIL returns true if a triple newT is already present in a set of triples createdPositives, otherwise false.
    */
    private static boolean is_already_present_inSet(Set<Triple_Integer> createdPositives, Triple_Integer newT) {
        boolean res=false;
        //linear search
        for(Triple_Integer t:createdPositives){
            if(Objects.equals(t.getSubject(), newT.getSubject()) && Objects.equals(t.getPredicate(), newT.getPredicate()) && Objects.equals(t.getObject(), newT.getObject())) {
                res = true;
                break;
            }
        }
        return res;
    }
    /*
    FUNCTION UTIL returns true if a triple newT is already present in a list of triples old_triples, otherwise false.
     */
    private static boolean is_already_present_inList(List<Triple_Integer> old_triples, Triple_Integer newT) {
        boolean res=false;

        //linear search in the list
        for(Triple_Integer t:old_triples){
            if(Objects.equals(t.getSubject(), newT.getSubject()) && Objects.equals(t.getPredicate(), newT.getPredicate()) && Objects.equals(t.getObject(), newT.getObject())) {
                res = true;
                break;
            }
        }
        return res;
    }
    /*
        FUNCTION UTIL to get the ID of the 'type' relation
    */
    private static void getTypeOf_ID(String path_file) throws IOException {
        List<String> relations = Files.readAllLines(Paths.get(path_file));
        relations=relations.subList(1, relations.size());
        for(String r:relations){
            String[] parts = r.split("\t");
            if (parts[0].equals(typeOf)) {
                IDTypeOf = Integer.parseInt(parts[1]);
                break;
            }
        }
    }

}
