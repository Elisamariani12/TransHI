import java.io.*;
import java.util.*;

import static java.util.Collections.max;

public class CC_Generator {
    private static final String folder_path="C:/Users/elisa/Desktop/ToUpload_COLAB_FILES/allfilestoRun-NELL/"; //number of random walks of K steps

    public static void main(String[] args) throws IOException {
        // Apri il file
        File file = new File(folder_path+"train2id_Consistent_withAugmentation.txt");
        BufferedReader reader = new BufferedReader(new FileReader(file));

        // Leggi il primo numero
        reader.readLine();

        // Leggi le triple
        List<Triple_Integer> triples = new ArrayList<>();
        Set<Integer> all_entities = new LinkedHashSet<>();
        String line;
        while ((line = reader.readLine()) != null) {
            String[] parts = line.split(" ");
            int subject = Integer.parseInt(parts[0]);
            int object = Integer.parseInt(parts[1]);
            int predicate = Integer.parseInt(parts[2]);
            all_entities.add(subject);
            all_entities.add(object);
            Triple_Integer mytriple = new Triple_Integer(subject, predicate, object);
            triples.add(mytriple);
        }
        // Chiudi il reader
        reader.close();

        //build adjacency matrix
        boolean[][] adj_matrix = new boolean[max(all_entities)+1][max(all_entities)+1];
        for(Triple_Integer t:triples){
            adj_matrix[t.getSubject()][t.getObject()]=true;
            adj_matrix[t.getObject()][t.getSubject()]=true;
        }

        Map<Integer, Set<Integer>> list_neighbors = get_neighbors(adj_matrix, all_entities);

        Map<Integer,Float> mapCC=createMapCC(all_entities.stream().toList(),triples,list_neighbors);

        print_on_file_CCMap(mapCC);

    }

    private static void print_on_file_CCMap(Map<Integer, Float> mapCC) {
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(folder_path+"CCs.txt"))) {
            for (Map.Entry<Integer, Float> entry : mapCC.entrySet()) {
                int key = entry.getKey();
                float value = entry.getValue();
                writer.write(key + " " + value);
                writer.newLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void CreateAll_Entites_and_triples(List<Triple_Integer> triples, List<Integer> all_entities) throws FileNotFoundException {

        // Open the file with all the training triples
        File file = new File("train2id.txt");
        Scanner scanner = new Scanner(file);

        // Leggi il primo numero
        int number_of_triples = scanner.nextInt();
        scanner.nextLine();

        while (scanner.hasNextLine()) {
            String line = scanner.nextLine();
            String[] parts = line.split(" ");
            int subject = Integer.parseInt(parts[0]);
            int object = Integer.parseInt(parts[1]);
            int predicate = Integer.parseInt(parts[2]);
            all_entities.add(subject);
            all_entities.add(object);
            Triple_Integer triple = new Triple_Integer(subject, predicate, object);
            triples.add(triple);
        }
        // Chiudi lo scanner
        scanner.close();
    }

    private static Map<Integer, Float> createMapCC(List<Integer> all_entities, List<Triple_Integer> triples, Map<Integer, Set<Integer>> list_neighbors) {
        Map<Integer, Float> mapCC=new HashMap<>();
        System.out.println("Creating MapCC");

        int percentage=0;
        for(Integer entity:all_entities){
            System.out.println("Percentage creation CCs:"+(float)100*percentage/ all_entities.size());
            List<Integer> neighbors;
            if(list_neighbors.get(entity)!=null) {
                neighbors = list_neighbors.get(entity).stream().toList();
            }
            else{
                neighbors=new ArrayList();
            }

            if (neighbors.size()<2){mapCC.put(entity,(float)0);continue;}

            int k=neighbors.size();
            int count=0;
            int index=0;
            for(Integer n1:neighbors){
                for(Integer n2:neighbors.subList(index+1,neighbors.size())){
                    if(list_neighbors.get(n1).contains(n2)||list_neighbors.get(n2).contains(n1)){
                        count++;
                    }
                }
                index++;
            }

            mapCC.put(entity,(float)count/(k*(k-1)/2));
            percentage++;
        }

        System.out.println("Finished creation CCs");

        return mapCC;
    }

    private static Map<Integer, Set<Integer>> get_neighbors(boolean[][] adj_matrix, Set<Integer> all_entities) {
        Map<Integer, Set<Integer>> dictionary_neighbors = new HashMap<>();

        Integer percentage=0;
        for (Integer entity : all_entities) {
            //System.out.println("Percentage neighbors creation:"+100*(float)percentage/all_entities.size());
            Set<Integer> myneighbors=new HashSet<>();

            for(int i=0;i<all_entities.size();i++){
                if(adj_matrix[entity][i])myneighbors.add(i);
            }

            dictionary_neighbors.put(entity,myneighbors);
            percentage++;
        }
        System.out.println("L'entità 0 ha come neighs:"+dictionary_neighbors.get(0));

        return dictionary_neighbors;
    }
}
