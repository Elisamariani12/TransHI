import org.semanticweb.owlapi.apibinding.OWLManager;
import org.semanticweb.owlapi.model.OWLAxiom;
import org.semanticweb.owlapi.model.OWLDataFactory;
import org.semanticweb.owlapi.model.OWLOntology;
import org.semanticweb.owlapi.model.OWLOntologyManager;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class Axiom_entailment {
    public static final String typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";

    public static String folder_path;

    public static void main(String[] args) throws Exception {
        folder_path=args[0];
        String name_ontology=args[1];

        // CREATE OWLOntologyManager + LOAD ONTOLOGY AND SAVE AXIOMS
        OWLOntologyManager manager = OWLManager.createOWLOntologyManager();
        OWLDataFactory dataFactory = manager.getOWLDataFactory();
        File inputOntologyFile = new File(folder_path+name_ontology);
        OWLOntology ontology = manager.loadOntologyFromOntologyDocument(inputOntologyFile);
        System.out.println("ontology loaded. Axioms counted: " + ontology.getAxiomCount());
        Set<OWLAxiom> axioms = ontology.getAxioms();
        Set<String> axioms_SetString = new HashSet<>();
        for (OWLAxiom axiom: axioms) axioms_SetString.add(axiom.toString());


        //load all the IDs of the relations & entities
        Map<String,Integer> IDsRelations = new HashMap<>();
        Map<String,Integer> IDsEntities = new HashMap<>();
        String filePath = folder_path+"relation2id.txt";
        IDsRelations = Files.lines(Paths.get(filePath))
                .skip(1)
                .map(line -> line.split("\t"))
                .collect(Collectors.toMap(
                        parts -> parts[0],
                        parts -> Integer.parseInt(parts[1])
                ));
        filePath = folder_path+"entity2id.txt";
        IDsEntities = Files.lines(Paths.get(filePath))
                .skip(1)
                .map(line -> line.split("\t"))
                .collect(Collectors.toMap(
                        parts -> parts[0],
                        parts -> Integer.parseInt(parts[1])
                ));

        //useful to keep the dictionary of equivalent classes here
        Map<String, Set<String>> dictionary_of_equivalentClasses= new HashMap<>();
        Map<String,Set<String>> dictionary_of_disjointclasses=new HashMap<>();
        Map<String,Set<String>> dictionary_of_subclasses=new HashMap<>();
        Map<String,Set<String>> dictionary_of_superclasses=new HashMap<>();
        Map<String,Set<String>> dictionary_of_subproperties=new HashMap<>();
        Map<String,Set<String>> dictionary_of_superproperties=new HashMap<>();
        Map<String,Set<String>> dictionary_of_equivalentproperties=new HashMap<>();
        Map<String,Set<String>> dictionary_of_inverseproperties=new HashMap<>();
        Set<String> set_of_irreflexiveproperties=new HashSet<>();
        Set<String> set_of_asymmetricproperties=new HashSet<>();


        //PUT DISJOINTWITH CLASSES, EQUIVALENT CLASSES, SUBCLASSES, SUPERCLASSES EQUIVALENTPROPERTIES, SUBPROPERTIES, INVERSE PROPERTIES  IN DICTIONARIES
        createDictionary(axioms_SetString,dictionary_of_disjointclasses,"DisjointClasses");
        createDictionary(axioms_SetString,dictionary_of_equivalentClasses,"EquivalentClasses");
        createDictionary(axioms_SetString,dictionary_of_subclasses,"SubClassOf");
        createDictionary(axioms_SetString,dictionary_of_inverseproperties,"InverseObjectProperties");
        createDictionary(axioms_SetString,dictionary_of_equivalentproperties,"EquivalentObjectProperties");
        createDictionary(axioms_SetString,dictionary_of_subproperties,"SubObjectPropertyOf");
        create_List(axioms_SetString,set_of_irreflexiveproperties,"IrreflexiveObjectProperty");
        create_List(axioms_SetString,set_of_asymmetricproperties,"AsymmetricObjectProperty");


        //-------------------------------STEP 0 --------------------------------------------------------------------------
        //APPLY REFLECTIVENESS FOR DISJOINTWITH,EQUIVALENTCLASSES,INVERSE PROPERTIES, EQUIVALENT PROPERTIES
        ApplySymmetry(dictionary_of_disjointclasses);
        ApplySymmetry(dictionary_of_equivalentClasses);
        ApplySymmetry(dictionary_of_inverseproperties);
        ApplySymmetry(dictionary_of_equivalentproperties);

        //APPLY TRANSITIVITY FOR DISJOINTWITH,EQUIVALENTCLASSES,INVERSE PROPERTIES, EQUIVALENT PROPERTIES, SUBCLASSES & SUBPROPERTIES
        //e.g. IF X IS SUBCLASS OF Y AND Y IS SUBCLASS OF Z, X IS SUBCLASS OF Z
        ApplyTransitiveClosure(dictionary_of_equivalentClasses);ApplySymmetry(dictionary_of_equivalentClasses);
        ApplyTransitiveClosure(dictionary_of_subclasses);
        ApplyTransitiveClosure(dictionary_of_equivalentproperties);ApplySymmetry(dictionary_of_equivalentproperties);
        ApplyTransitiveClosure(dictionary_of_subproperties);
        System.out.println("completed step 0");


        //------------------------------STEP 1 -------------------------------------------------------------------------
        // IF X IS SUBCLASS/SUBPROPERTY OF Y AND Y IS EQUIVALENT TO Z, X IS SUBCLASS/SUBPROPERTY OF Z
        Subkind_and_equivalence(dictionary_of_subclasses,dictionary_of_equivalentClasses);
        Subkind_and_equivalence(dictionary_of_subproperties,dictionary_of_equivalentproperties);
        System.out.println("completed step 1");

        /*------------------------------ STEP 2 : DISJOINT-WITH/INVERSEOF entailments ----------------------------------------*/
        // IF X IS DISJOINT-WITH/inverseOf Y and Y IS EQUIVALENT TO Z, X IS DISJOINT-WITH/InverseOf Z
        DisjORInverse_and_equivalence(dictionary_of_disjointclasses,dictionary_of_equivalentClasses);
        DisjORInverse_and_equivalence(dictionary_of_inverseproperties,dictionary_of_equivalentproperties);
        DisjORInverse_and_subkind(dictionary_of_disjointclasses,dictionary_of_subclasses);
        DisjORInverse_and_subkind(dictionary_of_inverseproperties,dictionary_of_subproperties);
        System.out.println("completed step 2");

        /*------------------------------ STEP 3 : DOMAIN&RANGE AXIOMS entailmemts (Added to the set of axioms and not the dictionaries like the others) -------------------------------------------*/
        AddAxioms_Domain_SubClasses_EquivalentClasses(axioms_SetString,dictionary_of_equivalentClasses,dictionary_of_subclasses);
        AddAxioms_Range_SubClasses_EquivalentClasses(axioms_SetString,dictionary_of_equivalentClasses,dictionary_of_subclasses);
        System.out.println("completed step 3");

        /*------------------------------ STEP 4 : IRREFLEXIVE/ASYMMETRIC entailmemts ---------------------------*/
        IrreflexORAsymm_and_equivalence(set_of_asymmetricproperties,dictionary_of_equivalentClasses);
        IrreflexORAsymm_and_equivalence(set_of_irreflexiveproperties,dictionary_of_equivalentClasses);
        IrreflexORAsymm_and_subprop(set_of_asymmetricproperties,dictionary_of_subproperties);
        IrreflexORAsymm_and_subprop(set_of_asymmetricproperties,dictionary_of_subproperties);
        System.out.println("completed step 4");

        /*------------------------------ STEP 5 : CREATE DICTIONARY OF SUPERCLASSES/SUPERPROPERTIES ---------------------------*/
        createDictionary_of_super(dictionary_of_subclasses,dictionary_of_superclasses);
        createDictionary_of_super(dictionary_of_subproperties,dictionary_of_superproperties);
        System.out.println("completed step 5");


        //-------------------------- STEP 6: PRINT THE AXIOMS IN SEPARATED FILES --------------------------------------------------
        print_dictionary_in_file(dictionary_of_superclasses,IDsEntities,"SuperClasses_axioms.txt");
        print_dictionary_in_file(dictionary_of_subclasses,IDsEntities,"SubClasses_axioms.txt");
        print_dictionary_in_file(dictionary_of_superproperties,IDsRelations,"SuperProperties_axioms.txt");
        print_dictionary_in_file(dictionary_of_equivalentClasses,IDsEntities,"EquivalentClasses_axioms.txt");
        print_dictionary_in_file(dictionary_of_equivalentproperties,IDsRelations,"EquivalentProperties_axioms.txt");
        print_dictionary_in_file(dictionary_of_disjointclasses,IDsEntities,"DisjointWith_axioms.txt");
        print_dictionary_in_file(dictionary_of_inverseproperties,IDsRelations,"InverseProperties_axioms.txt");
        print_domain_axioms_file(axioms_SetString,IDsRelations,IDsEntities,"Domain_axioms.txt");
        print_range_axioms_file(axioms_SetString,IDsRelations,IDsEntities,"Range_axioms.txt");
        print_set_in_file(set_of_irreflexiveproperties,IDsRelations,"IrreflexiveProperties_axioms.txt");
        print_set_in_file(set_of_asymmetricproperties,IDsRelations,"AsymmetricProperties_axioms.txt");

    }



    //------------- step 0 functions --------------------------------------------------------------------------------
    /*
    FUNCTION to create a dictionary for each type of axiom.
    input 1: set of all the axioms of the ontology
    input 2: dictionary to fill with the wanted axioms
    input 3: type of axioms taken into consideration
     */
    private static void createDictionary(Set<String> axioms_set, Map<String, Set<String>> dictionary, String axiom_type) {
        String substr;
        Matcher matcher;
        int index;
        Pattern pattern = Pattern.compile("<([^>]*)>");
        List<String> classes_in_the_axiom=new ArrayList<String>();
        Set<String> instances;

        //for all the axioms in the ontology
        for (String a  : axioms_set) {
            index = 0;
            substr = a.substring(0, a.indexOf("("));

            //see if they are of the type taken into consideration (axiom_type)
            if (substr.equals(axiom_type)) {
                matcher = pattern.matcher(a);
                while (matcher.find()) {
                    classes_in_the_axiom.add(index, matcher.group(1));
                    index++;
                }

                //add to the dictionary the axiom
                if(classes_in_the_axiom.size()>1) {
                    instances = dictionary.get(classes_in_the_axiom.get(1));
                    if (instances != null) {
                        instances.add(classes_in_the_axiom.get(0));
                    } else {
                        HashSet<String> newLineInTheDictionary = new HashSet<>();
                        newLineInTheDictionary.add(classes_in_the_axiom.get(0));
                        dictionary.put(classes_in_the_axiom.get(1), newLineInTheDictionary);
                    }
                }

            }
            classes_in_the_axiom.clear();
        }
    }

    /*
    FUNCTION to create two lists: one of all the relations that are irreflexive and one for those that are asymmetric
    input 1: set of all the axioms of the ontology
    input 2: list to fill with the wanted axioms
    input 3: type of axioms taken into consideration
     */
    private static void create_List(Set<String> axioms_set, Set<String> set_to_fill, String axiom_type) {
        String substr;
        Matcher matcher;
        int index;
        Pattern pattern = Pattern.compile("<([^>]*)>");
        List<String> classes_in_the_axiom=new ArrayList<String>();
        Set<String> instances;

        //for all the axioms in the ontology

        for (String a  : axioms_set) {
            index = 0;
            substr = a.substring(0, a.indexOf("("));
            //see if they are of the type taken into consideration (axiom_type)

            if (substr.equals(axiom_type)) {
                matcher = pattern.matcher(a);
                while (matcher.find()) {
                    classes_in_the_axiom.add(index, matcher.group(1));
                    index++;
                }
                //add to the list the axiom

                if(classes_in_the_axiom.size()>0) {
                    set_to_fill.add(classes_in_the_axiom.get(0));
                }

            }
            classes_in_the_axiom.clear();
        }
    }

    /*
    FUNCTION to insert in the dictionaries new axioms derived with the entailments of the type:
    if x-rel.->y then y-rel.->x
    e.g. if x-Disj.Class->y then y-Disj.Class->x
     */
    private static void ApplySymmetry(Map<String, Set<String>> dictionary) {
        int new_additions=0;
        Map<String,Set<String>> dictionary_copy=new HashMap<>(dictionary);

        do{
            new_additions=0;
            Map<String,Set<String>> dictionary_temp=new HashMap<>(dictionary_copy);

            //for every entry in the dictionary, if the reflexive axioms is not already contained, add it
            for(String key:dictionary_copy.keySet()){
                for(String value:dictionary_copy.get(key)){
                    if(!dictionary_temp.containsKey(value)){
                        dictionary_temp.put(value,new HashSet<>());
                    }
                    if(dictionary_temp.get(value).add(key))new_additions++;
                }
            }
            dictionary_copy=dictionary_temp;
        }while(new_additions>0);

        dictionary.clear();
        dictionary.putAll(dictionary_copy);
    }

    /*
    FUNCTION to insert in the dictionaries new axioms derived with the entailments of the type:
    if x-rel.->y and y-rel.->z then x-rel.->z
    e.g. if x-subClass->y and y-subClass->z then x-subClass->z
     */
    private static void ApplyTransitiveClosure(Map<String, Set<String>> dictionary) {
        int new_additions = 0;
        Map<String, Set<String>> dictionary_copy = new HashMap<>(dictionary);

        do {
            new_additions = 0;
            Map<String, Set<String>> tempAdditions = new HashMap<>();

            //for every entry add the new entailed axiom
            for (String key : dictionary_copy.keySet()) {
                for (String value : dictionary_copy.get(key)) {
                    if (dictionary_copy.containsKey(value)) {
                        tempAdditions.putIfAbsent(key, new HashSet<>());
                        for (String value2 : dictionary_copy.get(value)) {
                            tempAdditions.get(key).add(value2);
                        }
                    }
                }
            }

            // Apply all additions after the iteration
            for (String key : tempAdditions.keySet()) {
                if(dictionary_copy.get(key).addAll(tempAdditions.get(key)))new_additions++;
            }

        } while (new_additions > 0);

        dictionary.clear();
        dictionary.putAll(dictionary_copy);
    }

    //------------ step 1 functions ---------------------------------------------------------------------------------
    /*
    FUNCTION to entail the two axioms:
        1) if x-subclass->y and y-equiv.class->z then x-subclass->z
        2) if x-subclass->y and x-equiv.class->z then z-subclass->y
     and the same for subproperties!
    input 1) dictionary containing the axioms of subclass/subproperties
    input 2) dictionary containing the axioms of equivalent class/ properties
     */
    private static void Subkind_and_equivalence(Map<String, Set<String>> dictionaryOfSubkind, Map<String, Set<String>> dictionaryOfEquivalents) {
        Map<String, Set<String>> dictionary_copy = new HashMap<>(dictionaryOfSubkind);
        int new_additions;


        //AXIOMS ENTAILED: if x-subclass->y and y-equiv.class->z then x-subclass->z
        do {
            new_additions = 0;
            Map<String, Set<String>> dictionarytemp = new HashMap<>(dictionary_copy);
            ApplyTransitiveClosure(dictionarytemp);

            Map<String, Set<String>> changes = new HashMap<>();
            for (String key : dictionarytemp.keySet()) {
                if (dictionaryOfEquivalents.containsKey(key)) {
                    for (String equivalent : dictionaryOfEquivalents.get(key)) {
                        changes
                                .computeIfAbsent(equivalent, k -> new HashSet<>())
                                .addAll(dictionarytemp.get(key));
                    }
                }
            }
            for (String key : changes.keySet()) {
                Set<String> currentSet = dictionary_copy.computeIfAbsent(key, k -> new HashSet<>());
                if (currentSet.addAll(changes.get(key))) {
                    new_additions++;
                }
            }
            dictionary_copy.putAll(dictionarytemp);

        } while (new_additions > 0);

        ApplyTransitiveClosure(dictionary_copy);
        dictionaryOfSubkind.clear();
        dictionaryOfSubkind.putAll(dictionary_copy);
        ApplyTransitiveClosure(dictionaryOfSubkind);
        dictionary_copy = new HashMap<>(dictionaryOfSubkind);

        //AXIOMS ENTAILED: if x-subclass->y and x-equiv.class->z then z-subclass->y
        do {
            new_additions = 0;
            ApplyTransitiveClosure(dictionary_copy);
            Map<String, Set<String>> updates = new HashMap<>();

            for (String key : dictionary_copy.keySet()) {
                for (String subclass : dictionary_copy.get(key)) {
                    if (dictionaryOfEquivalents.containsKey(subclass)) {
                        updates
                                .computeIfAbsent(key, k -> new HashSet<>())
                                .addAll(dictionaryOfEquivalents.get(subclass));
                    }
                }
            }
            for (Map.Entry<String, Set<String>> entry : updates.entrySet()) {
                if (dictionary_copy.get(entry.getKey()).addAll(entry.getValue())) {
                    new_additions++;
                }
            }

        } while (new_additions > 0);

        dictionaryOfSubkind.clear();
        dictionaryOfSubkind.putAll(dictionary_copy);
        ApplyTransitiveClosure(dictionaryOfSubkind);
    }


    //------------ step 2 functions ---------------------------------------------------------------------------------
    /*
    FUNCTION to entail the axiom:
        1)  IF X IS DISJOINT-WITH/inverseOf Y and Y IS EQUIVALENT TO Z, X IS DISJOINT-WITH/
    input 1) dictionary containing the axioms of disjjointwith/inverseof
    input 2) dictionary containing the axioms of equivalent class/ properties
     */
    private static void DisjORInverse_and_equivalence(Map<String, Set<String>> dictionary, Map<String, Set<String>> dictionaryOfEquivalents) {
        int new_additions=0;
        Map<String, Set<String>> dictionary_copy = new HashMap<>(dictionary);
        do {
            new_additions = 0;
            Map<String, Set<String>> dictionarytemp = new HashMap<>(dictionary_copy);

            for (String key : dictionary_copy.keySet()) {
                if (dictionaryOfEquivalents.containsKey(key)) {
                    for (String equivalent : dictionaryOfEquivalents.get(key)) {
                        dictionarytemp.putIfAbsent(equivalent, new HashSet<>());
                        if (dictionarytemp.get(equivalent).addAll(dictionarytemp.get(key)))new_additions++;
                    }
                }
            }
            dictionary_copy=dictionarytemp;
            ApplySymmetry(dictionary_copy);
        }while(new_additions>0);

        dictionary.clear();
        dictionary.putAll(dictionary_copy);
    }
    /*
    FUNCTION to entail the axiom:
        1)  IF X IS DISJOINT-WITH/inverseOf Y and Z IS subclass/subprop TO X, Z IS DISJOINT-WITH/inverseof Y
    input 1) dictionary containing the axioms of disjjointwith/inverseof
    input 2) dictionary containing the axioms of sub classes/ properties
     */
    private static void DisjORInverse_and_subkind(Map<String, Set<String>> dictionary, Map<String, Set<String>> dictionaryOfSubkind) {
        int new_additions=0;
        Map<String, Set<String>> dictionary_copy = new HashMap<>(dictionary);
        do {
            new_additions = 0;
            Map<String, Set<String>> dictionarytemp = new HashMap<>(dictionary_copy);

            for (String key : dictionary_copy.keySet()) {
                if (dictionaryOfSubkind.containsKey(key)) {
                    for (String subclass : dictionaryOfSubkind.get(key)) {
                        dictionarytemp.putIfAbsent(subclass, new HashSet<>());
                        if (dictionarytemp.get(subclass).addAll(dictionarytemp.get(key))) {new_additions++;}
                    }
                }
            }

            dictionary_copy=dictionarytemp;
            ApplySymmetry(dictionary_copy);
        }while(new_additions>0);


        dictionary.clear();
        dictionary.putAll(dictionary_copy);
    }

    // ----------- step 3 functions ------------------------------------------------------------------------------
    /*
    FUNCTION to entail the axiom:
        1)  IF X is in domain of R and x IS subclass TO Y, Y is in domain of R
        1)  IF X is in domain of R and x IS equivclass TO Y, Y is in domain of R
    input 1) set of all the axioms
    input 2) dictionary containing the axioms of equiv classes
    input 3) dictionary containing the axioms of sub classes
    The entailed axioms are added to the set of all the axioms
     */
    private static void AddAxioms_Domain_SubClasses_EquivalentClasses(Set<String> axioms_set, Map<String, Set<String>> dictionary_of_equivalentClasses, Map<String, Set<String>> dictionary_of_subclasses) {

        String substr;
        Pattern pattern = Pattern.compile("<([^>]*)>");
        Matcher matcher;
        List<String> classes_in_the_axiom=new ArrayList<String>();
        int index=0;
        Set<String> newAxioms = new HashSet<>();
        String to_Add;


        for (String a  : axioms_set) {
            index=0;
            substr = a.substring(0, a.indexOf("("));
            if (substr.equals("ObjectPropertyDomain")||substr.equals("AnnotationPropertyDomain")) {
                matcher = pattern.matcher(a);
                while (matcher.find()) {
                    classes_in_the_axiom.add(index, matcher.group(1));
                    index++;
                }
                if(classes_in_the_axiom.size()>1) {

                    //if i have some equivalent classes...
                    if (dictionary_of_equivalentClasses.get(classes_in_the_axiom.get(1)) != null) {
                        for (String equivalent_class : dictionary_of_equivalentClasses.get(classes_in_the_axiom.get(1))) {
                            to_Add = "ObjectPropertyDomain(<" + classes_in_the_axiom.get(0) + "> <" + equivalent_class + ">)";
                            //System.out.println(to_Add);
                            newAxioms.add(to_Add);
                        }
                    }

                    //if i have some subclasses...
                    if (dictionary_of_subclasses.get(classes_in_the_axiom.get(1)) != null) {
                        // i'm going to substitute it with its subclasses
                        for (String subclass : dictionary_of_subclasses.get(classes_in_the_axiom.get(1))) {

                            to_Add = "ObjectPropertyDomain(<" + classes_in_the_axiom.get(0) + "> <" + subclass + ">)";
                            newAxioms.add(to_Add);
                        }
                    }
                }
            }
            classes_in_the_axiom.clear();
        }

        //System.out.println("ad ora gli assiomi che aggiungiamo sono"+newAxioms.size());
        //System.out.println(newAxioms);
        axioms_set.addAll(newAxioms);
    }
    /*
    FUNCTION to entail the axiom:
        1)  IF X is in range of R and x IS subclass TO Y, Y is in range of R
        1)  IF X is in range of R and x IS equivclass TO Y, Y is in range of R
    input 1) set of all the axioms
    input 2) dictionary containing the axioms of equiv classes
    input 3) dictionary containing the axioms of sub classes
     The entailed axioms are added to the set of all the axioms
     */
    private static void AddAxioms_Range_SubClasses_EquivalentClasses(Set<String> axioms_set, Map<String, Set<String>> dictionary_of_equivalentClasses, Map<String, Set<String>> dictionary_of_subclasses) {

        String substr;
        Pattern pattern = Pattern.compile("<([^>]*)>");
        Matcher matcher;
        List<String> classes_in_the_axiom=new ArrayList<String>();
        int index=0;
        Set<String> newAxioms = new HashSet<>();
        String to_Add;

        // Now I'm going to create, for each disjointwith axiom, other disjoint axioms between the equivalent
        // classes && Subclasses of the classes of that axiom
        for (String a  : axioms_set) {
            index=0;
            substr = a.substring(0, a.indexOf("("));
            if (substr.equals("ObjectPropertyRange")||substr.equals("AnnotationPropertyRange")) {
                matcher = pattern.matcher(a);
                while (matcher.find()) {
                    classes_in_the_axiom.add(index, matcher.group(1));
                    index++;
                }
                if(classes_in_the_axiom.size()>1) {
                    //if i have some equivalent classes...
                    if (dictionary_of_equivalentClasses.get(classes_in_the_axiom.get(1)) != null) {
                        for (String equivalent_class : dictionary_of_equivalentClasses.get(classes_in_the_axiom.get(1))) {
                            to_Add = "ObjectPropertyRange(<" + classes_in_the_axiom.get(0) + "> <" + equivalent_class + ">)";
                            newAxioms.add(to_Add);
                        }
                    }

                    //if i have some subclasses...
                    if (dictionary_of_subclasses.get(classes_in_the_axiom.get(1)) != null) {
                        // i'm going to substitute it with its subclasses
                        for (String subclass : dictionary_of_subclasses.get(classes_in_the_axiom.get(1))) {
                            //i'm going to create the axioms of disjunction between the subclasses and the class in disjoint with
                            to_Add = "ObjectPropertyRange(<" + classes_in_the_axiom.get(0) + "> <" + subclass + ">)";
                            newAxioms.add(to_Add);
                        }
                    }
                }
            }
            classes_in_the_axiom.clear();
        }

        //System.out.println("ad ora gli assiomi che aggiungiamo sono"+newAxioms.size());
        //System.out.println(newAxioms);
        axioms_set.addAll(newAxioms);
    }

    // ----------- step 4 functions ------------------------------------------------------------------------------
    /*
    FUNCTION to entail the axiom:
        1)  IF p is asymm/irreflex p2 IS equiv.prop TO p, p2 is asymm/irreflex
    input 1) list containing the asymm/irreflex properties
    input 2) dictionary containing the axioms of equiv classes
     */
    private static void IrreflexORAsymm_and_equivalence(Set<String> setOfAsymmOrIrreflproperties, Map<String, Set<String>> dictionaryOfEquivalentClasses) {
        for(String s:setOfAsymmOrIrreflproperties){
            if(dictionaryOfEquivalentClasses.containsKey(s)){
                for(String equiv:dictionaryOfEquivalentClasses.get(s)){
                    setOfAsymmOrIrreflproperties.add(equiv);
                }
            }
        }
    }
    /*
       FUNCTION to entail the axiom:
           1)  IF p is asymm/irreflex p2 IS subprop TO p, p2 is asymm/irreflex
       input 1) list containing the asymm/irreflex properties
       input 2) dictionary containing the axioms of subprop
        */
    private static void IrreflexORAsymm_and_subprop(Set<String> setOfAsymmORIrreflproperties, Map<String, Set<String>> dictionaryOfSubproperties) {
        for(String s:setOfAsymmORIrreflproperties){
            if(dictionaryOfSubproperties.containsKey(s)){
                for(String sub:dictionaryOfSubproperties.get(s)){
                    setOfAsymmORIrreflproperties.add(sub);
                }
            }
        }
    }
    // ----------- step 5 functions ------------------------------------------------------------------------------
     /*
    FUNCTION to link an entity to all its superclasses. It's the opposite of the dictionary_of_sub where each entity is linked to all its subclasses
     */
    public static void createDictionary_of_super(Map<String, Set<String>> dictionary_of_sub, Map<String, Set<String>> dictionary_of_sup) {
        for (Map.Entry<String, Set<String>> entry : dictionary_of_sub.entrySet()) {
            for (String subclass : entry.getValue()) {
                dictionary_of_sup.computeIfAbsent(subclass, k -> new HashSet<>()).add(entry.getKey());
            }
        }
    }


    //-------------- step 6 functions to print in files  --------------------------------------------------------------------------
    private static void print_dictionary_in_file(Map<String, Set<String>> dictionary, Map<String, Integer> iDsRelations,String file_name) throws IOException {
        File file = new File(folder_path+file_name);
        System.out.println("I deleted the file because it was already existing:"+file.delete());
        FileWriter writer = new FileWriter(folder_path+file_name,true);
        BufferedWriter bw = new BufferedWriter(writer);


        for(String key:dictionary.keySet()){
            if(iDsRelations.get("<"+key+">")!=null) {
                Set<Integer> values=new HashSet<>();
                for(String value:dictionary.get(key)){
                    if(!value.equals(key) && iDsRelations.get("<" + value + ">")!=null)values.add(iDsRelations.get("<" + value + ">"));
                }
                if(values.size()>0) {
                    bw.write(String.valueOf(iDsRelations.get("<" + key + ">")) + ",");
                    for (int value : values) {
                        bw.write(String.valueOf(value) + ",");
                    }
                    bw.newLine();
                }
            }
        }
        bw.close();
    }
    /*
   FUNCTION to extract the domain/range axioms from the set of all the axioms (that contain the entailed ones) and convert them into their IDs version instead of ttl
    input 1) set of the axioms domain/range
    input 2) set of all the axioms
    input 3) map of the entities in strings and their IDs
    input 4) map of the relations in strings and their IDs
    */
    private static HashMap<Integer,Set<Integer>> extractAxioms_rel_entity(String type_axiom_string, Set<String> axioms_setString,Map<String,Integer> map_IDs_relations, Map<String,Integer> map_IDs_entities) {
        String substr;
        Pattern pattern = Pattern.compile("<([^>]*)>");
        Matcher matcher;
        List<String> classes_in_the_axiom=new ArrayList<String>();
        int index=0;
        Set<String> newAxioms = new HashSet<>();
        String to_Add;
        HashMap<Integer,Set<Integer>> IDsMap=new HashMap<>();

        for (String a  : axioms_setString) {
            index=0;
            substr = a.substring(0, a.indexOf("("));
            if (substr.equals(type_axiom_string)) {
                matcher = pattern.matcher(a);
                while (matcher.find()) {
                    classes_in_the_axiom.add(index, matcher.group(1));
                    index++;
                }
                try {
                    int first_id = map_IDs_relations.get("<" + classes_in_the_axiom.get(0) + ">");
                    int second_id = map_IDs_entities.get("<" + classes_in_the_axiom.get(1) + ">");
                    if (IDsMap.containsKey(first_id)) {
                        IDsMap.get(first_id).add(second_id);
                    } else {
                        Set<Integer> newset = new HashSet<>();
                        newset.add(second_id);
                        IDsMap.put(first_id, newset);
                    }
                }
                catch(Exception e){}
            }
            classes_in_the_axiom.clear();
        }
        return IDsMap;
    }
    private static void print_domain_axioms_file(Set<String> axioms_setString,Map<String,Integer> map_IDs_relations, Map<String,Integer> map_IDs_entities,String file_name) throws IOException {
        File file = new File(folder_path+file_name);
        System.out.println("I deleted the file because it was already existing:"+file.delete());
        FileWriter writer = new FileWriter(folder_path+file_name,true);
        BufferedWriter bw = new BufferedWriter(writer);

        HashMap<Integer,Set<Integer>> extracted_disjointWith_axioms=extractAxioms_rel_entity("ObjectPropertyDomain",axioms_setString,map_IDs_relations,map_IDs_entities);

        for(int key:extracted_disjointWith_axioms.keySet()){
            bw.write(String.valueOf(key)+",");
            for(int value:extracted_disjointWith_axioms.get(key)){
                bw.write(String.valueOf(value)+",");
            }
            bw.newLine();
        }

        extracted_disjointWith_axioms=extractAxioms_rel_entity("AnnotationPropertyDomain",axioms_setString,map_IDs_relations,map_IDs_entities);
        for(int key:extracted_disjointWith_axioms.keySet()){
            bw.write(String.valueOf(key)+",");
            for(int value:extracted_disjointWith_axioms.get(key)){
                bw.write(String.valueOf(value)+",");
            }
            bw.newLine();
        }

        bw.close();
    }
    private static void print_range_axioms_file(Set<String> axioms_setString,Map<String,Integer> map_IDs_relations, Map<String,Integer> map_IDs_entities,String file_name) throws IOException {
        File file = new File(folder_path+file_name);
        System.out.println("I deleted the file because it was already existing:"+file.delete());
        FileWriter writer = new FileWriter(folder_path+file_name,true);
        BufferedWriter bw = new BufferedWriter(writer);

        HashMap<Integer,Set<Integer>> extracted_disjointWith_axioms=extractAxioms_rel_entity("ObjectPropertyRange",axioms_setString,map_IDs_relations,map_IDs_entities);

        for(int key:extracted_disjointWith_axioms.keySet()){
            bw.write(String.valueOf(key)+",");
            for(int value:extracted_disjointWith_axioms.get(key)){
                bw.write(String.valueOf(value)+",");
            }
            bw.newLine();
        }

        extracted_disjointWith_axioms=extractAxioms_rel_entity("AnnotationPropertyRange",axioms_setString,map_IDs_relations,map_IDs_entities);

        for(int key:extracted_disjointWith_axioms.keySet()){
            bw.write(String.valueOf(key)+",");
            for(int value:extracted_disjointWith_axioms.get(key)){
                bw.write(String.valueOf(value)+",");
            }
            bw.newLine();
        }

        bw.close();
    }
    private static void print_set_in_file(Set<String> my_Set, Map<String, Integer> iDsRelations, String file_name) throws IOException {
        File file = new File(folder_path+file_name);
        System.out.println("I deleted the file because it was already existing:"+file.delete());
        FileWriter writer = new FileWriter(folder_path+file_name,true);
        BufferedWriter bw = new BufferedWriter(writer);

        for(String s:my_Set){
            if(iDsRelations.get("<"+s+">")!=null) {
                bw.write(String.valueOf(iDsRelations.get("<" + s + ">")));
                bw.newLine();
            }
        }
        bw.close();
    }
    private static void printDictionary_for_debugging(Map<String,Set<String>> dic){
        for(Map.Entry<String,Set<String>> entry:dic.entrySet()){
            System.out.println(entry);
        }
    }

}
