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
    public static String folder_path; //="C:/Users/elisa/Desktop/ToUpload_COLAB_FILES/allfilestoRun-NELL/";      //to run it in intellij is GENERATOR-MATERIAL/
    public static String filetoCreate; //="valid2id_Consistent.txt";
    public static String filetoConvert; //="consistent_triples_valid.ttl";

    public static void main(String[] args) throws Exception {
        folder_path=args[0];
        filetoCreate=args[1];
        filetoConvert=args[2];

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
        List<Triple_Integer> listPositives_triplesInteger=Convert(listPositives_triples,IDsEntities,IDsRelations);

        PrintConvertedTriplesInFile(listPositives_triplesInteger,LabelsMap);
    }


    private static List<Triple_Integer> Convert(List<Triple> listTriplesString, Map<String,Integer> iDsEntities, Map<String, Integer> iDsRelations) {
        List<Triple_Integer> listTriplesID=new ArrayList<>();

        for(Triple t: listTriplesString){
            Triple_Integer converted_t=new Triple_Integer(iDsEntities.get(t.getSubject()),iDsRelations.get(t.getPredicate()),iDsEntities.get(t.getObject()));
            listTriplesID.add(converted_t);
        }

        return listTriplesID;
    }

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
