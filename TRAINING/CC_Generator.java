import java.io.*;
import java.util.*;

import static java.util.Collections.max;

public class CC_Generator {
    private static String folder_path; //folder with train2id_Consistent_withAugmentation.txt

    public static void main(String[] args) throws IOException {
        folder_path=args[0];

        // open the file
        File file = new File(folder_path+"train2id_Consistent_withAugmentation.txt");
        BufferedReader reader = new BufferedReader(new FileReader(file));

        // read first number
        reader.readLine();

        // read the triples
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

        // Close the reader
        reader.close();

        //build adjacency matrix
        boolean[][] adj_matrix = new boolean[max(all_entities)+1][max(all_entities)+1];
        for(Triple_Integer t:triples){
            adj_matrix[t.getSubject()][t.getObject()]=true;
            adj_matrix[t.getObject()][t.getSubject()]=true;
        }

        //get neighbors for each entity
        Map<Integer, Set<Integer>> list_neighbors = get_neighbors(adj_matrix, all_entities);

        //get cc for each entity
        Map<Integer,Float> mapCC=createMapCC(all_entities.stream().toList(),triples,list_neighbors);

        //print CCs in a file
        print_on_file_CCMap(mapCC);

    }

    /*
    FUNCTION UTIL to print the CC related to each entity in a file
     */
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

    /*
    FUNCTION to compute the CC related to each entity
    input 1: A list containing the IDs of all entities.
    input 2: A list of triples (not used in this function).
    input 3: A map of entity IDs to a set of their neighbors.
    output: A map of entity IDs to their clustering coefficients.
     */
    private static Map<Integer, Float> createMapCC(List<Integer> all_entities, List<Triple_Integer> triples, Map<Integer, Set<Integer>> list_neighbors) {
        // Initialize the output map.
        Map<Integer, Float> mapCC=new HashMap<>();
        System.out.println("Creating MapCC");

        int percentage=0;
        // Iterate over all the entities.
        for(Integer entity:all_entities){
            System.out.println("Percentage creation CCs:"+(float)100*percentage/ all_entities.size());

            // Retrieve the neighbors of the current entity.
            List<Integer> neighbors;
            if(list_neighbors.get(entity)!=null) {
                neighbors = list_neighbors.get(entity).stream().toList();
            }
            else{
                neighbors=new ArrayList();
            }

            // If the entity has fewer than two neighbors, its clustering coefficient is 0.
            if (neighbors.size()<2){mapCC.put(entity,(float)0);continue;}

            int k=neighbors.size();
            int count=0;
            int index=0;
            // Count how many pairs of neighbors are connected.
            for(Integer n1:neighbors){
                for(Integer n2:neighbors.subList(index+1,neighbors.size())){
                    if(list_neighbors.get(n1).contains(n2)||list_neighbors.get(n2).contains(n1)){
                        count++;
                    }
                }
                index++;
            }

            // Compute the clustering coefficient using the formula.
            mapCC.put(entity,(float)count/(k*(k-1)/2));     //cc formula
            percentage++;
        }

        System.out.println("Finished creation CCs");

        return mapCC;
    }

    /*
    FUNCTION to get the neighbors (1 hop) of each entity
    input 1: adjacency matrix containing all the neighbors in the KG
    input 2: list of all the entities in the KG
    output: map of each entity and the set of its direct neighbors
     */
    private static Map<Integer, Set<Integer>> get_neighbors(boolean[][] adj_matrix, Set<Integer> all_entities) {
        Map<Integer, Set<Integer>> dictionary_neighbors = new HashMap<>();

        Integer percentage=0;
        //forall the entities in the kg
        for (Integer entity : all_entities) {
            Set<Integer> myneighbors=new HashSet<>();

            //if the adjacency matrix is true in [entity][i] means that i is a neighbor
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
