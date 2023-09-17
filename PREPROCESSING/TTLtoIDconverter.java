import java.awt.*;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.util.List;
import java.util.stream.Collectors;

public class TTLtoIDconverter {
    public static String folder_path; // folder with relation2id,entity2id,the file to convert
    public static String filetoCreate; // file of converted triples
    public static String filetoConvert; //file to convert from triples in ttl to triples with IDs

    public static void main(String[] args) throws Exception {
        folder_path=args[0];
        filetoConvert=args[1];

        //choose name of output file
        if(filetoConvert.equals("consistent_triples_train.ttl"))filetoCreate="train2id_Consistent.txt";
        else if(filetoConvert.equals("consistent_triples_test.ttl"))filetoCreate="test2id_Consistent.txt";
        else if(filetoConvert.equals("consistent_triples_valid.ttl"))filetoCreate="valid2id_Consistent.txt";


        //read all the relations and entities of the KG
        Map<String,Integer> IDsRelations = new HashMap<>();
        Map<String, Integer> IDsEntities = new HashMap<>();
        Map<Triple_Integer,Integer> LabelsMap = new HashMap<>();
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

        //read the triples of the file to convert from ttl version to IDs  in "listPositives_triples"
        if(filetoConvert.equals("consistent_triples_test.ttl")) {
            filePath = folder_path+"test2id_all.txt";
            LabelsMap = Files.lines(Paths.get(filePath))
                    .skip(1)
                    .map(line -> line.split("\t"))
                    .collect(Collectors.toMap(
                            parts -> new Triple_Integer(
                                    Integer.parseInt(parts[1].split(" ")[0]),
                                    Integer.parseInt(parts[1].split(" ")[2]),
                                    Integer.parseInt(parts[1].split(" ")[1])
                            ),
                            parts -> Integer.parseInt(parts[0])
                    ));
        }
        List<String> listPositives = new ArrayList<String>();
        listPositives = Files.readAllLines(Paths.get(folder_path+filetoConvert));
        List<Triple> listPositives_triples=new ArrayList<>();
        for(String s: listPositives){
            String[] parts = s.split(" ");
            Triple t=new Triple(parts[0],parts[1],parts[2].substring(0,parts[2].length()-1));
            listPositives_triples.add(t);
        }

        //convert all the triples
        List<Triple_Integer> listPositives_triplesInteger=Convert(listPositives_triples,IDsEntities,IDsRelations);

        PrintConvertedTriplesInFile(listPositives_triplesInteger,LabelsMap);
    }

    /*
       FUNCTION to convert triples from ttl to IDs
       input 1: list of the triples written in ttl
       input 2: map of the string of the entities with their IDs
       input 2: map of the string of the relations with their IDs
        */
    private static List<Triple_Integer> Convert(List<Triple> listTriplesString, Map<String,Integer> iDsEntities, Map<String, Integer> iDsRelations) {
        List<Triple_Integer> listTriplesID=new ArrayList<>();

        //convert each triple
        for(Triple t: listTriplesString){
            Triple_Integer converted_t=new Triple_Integer(iDsEntities.get(t.getSubject()),iDsRelations.get(t.getPredicate()),iDsEntities.get(t.getObject()));
            listTriplesID.add(converted_t);
        }

        return listTriplesID;
    }

    /*
    FUNCTION to print converted triples in a file
    input 1: list of the triples written with IDs
    input 2: if the file is the one of test data each entity has a label that is used for the task of link prediction so we re-apply the label that was removed in the conversion from IDs to TTL
     */
    private static void PrintConvertedTriplesInFile(List<Triple_Integer> listTriplesID, Map<Triple_Integer, Integer> labelsMap) throws IOException {
        File file = new File(folder_path + filetoCreate);
        System.out.println("I deleted the file because it was already existing:" + file.delete());
        FileWriter writer = new FileWriter(folder_path + filetoCreate, true);
        BufferedWriter bw = new BufferedWriter(writer);

        bw.write(String.valueOf(listTriplesID.size()));
        bw.newLine();

        for (Triple_Integer t : listTriplesID) {

            if(filetoConvert.equals("consistent_triples_test.ttl")){
                for(Triple_Integer k:labelsMap.keySet()){
                    if(Objects.equals(k.getSubject(), t.getSubject()) && Objects.equals(k.getObject(), t.getObject()) && Objects.equals(k.getPredicate(), t.getPredicate())){
                        bw.write(String.valueOf(labelsMap.get(k)));
                        bw.write("\t");
                        break;
                    }
                }
            }
            bw.write(t.getSubject() + " " + t.getObject() + " " + t.getPredicate());
            if(listTriplesID.get(listTriplesID.size()-1)!=t)bw.newLine();
        }

        bw.close();
    }
}
