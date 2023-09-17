import javax.swing.plaf.basic.BasicSplitPaneUI;
import java.io.*;
import java.util.*;

import static java.util.Collections.max;

public class NeighborsGenerator {

    private static Integer K ;  //k to find the k-hops neighbors
    private static Integer omega; //number of random walks of K steps

    private static String folder_path;   //folder containing train2id_Consistent_withAugmentation.txt


    public static void main(String[] args) throws IOException {
        //get parameters
        K = Integer.parseInt(args[0]);
        omega = Integer.parseInt(args[1]);
        folder_path= args[2];

        // open the file
        File file = new File(folder_path+"train2id_Consistent_withAugmentation.txt");
        BufferedReader reader = new BufferedReader(new FileReader(file));

        // read the first line (with the number of triples)
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

        //print neighbors in file
        try (PrintWriter writer = new PrintWriter(folder_path+"file_Neighbors.txt")) {
            for (Map.Entry<Integer, Set<Integer>> entry : list_neighbors.entrySet()) {
                StringBuilder sb = new StringBuilder();
                sb.append(entry.getKey()).append(",");
                for (Integer value : entry.getValue()) {
                    sb.append(value).append(",");
                }
                writer.println(sb);
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    /*
    FUNCTION to get the k-hop neighbors of each entity using 'omega' random walks.
    input 1: adjacency matrix containing all the neighbors in the KG
    input 2: list of all the entities in the KG
     */
    private static Map<Integer, Set<Integer>> get_neighbors(boolean[][] adj_matrix, Set<Integer> all_entities) {
        Map<Integer, Set<Integer>> dictionary_neighbors = new HashMap<>();
        List<Integer> lenghts_found_neighbors=new ArrayList<>();

        Integer percentage=0;
        // for each entity of the graph search the k-hop neighbors
        for (Integer entity : all_entities) {

            // print % of completion
            System.out.println("Percentage neighbors creation:" + 100 * (float) percentage / all_entities.size());

            // set to temporarily save the neighbors of an entity
            Set<Integer> myneighbors = new HashSet<>();

            int num_random_walks = 0;
            int current_entity = entity;

            // do omega random walks for each entity
            while (num_random_walks < omega) {

                int current_level = 0;
                int last_entity = -1; // initialize the last seen entity as -1

                // do random walk for k steps
                while (current_level < K) {

                    List<Integer> current_neighbors = new ArrayList<>();

                    // find neighbors of current entity exploiting adjacency matrix
                    for (int i = 0; i < adj_matrix[current_entity].length; i++) {
                        if (adj_matrix[current_entity][i]) {
                            if (last_entity != i) current_neighbors.add(i);
                        }
                    }

                    // if there are available neighbors
                    if (current_neighbors.size() > 0) {

                        // choose 1 in a random way
                        Random random = new Random();
                        int randomIndex = random.nextInt(current_neighbors.size());
                        int chosen__next__entity = current_neighbors.get(randomIndex);

                        // Se si sono fatti k hops, si è trovato un k-hop neighbor. otherwise, keep searching deeper
                        if (current_level == K - 1) {
                            myneighbors.add(chosen__next__entity);
                        } else {
                            last_entity = current_entity;
                            current_entity = chosen__next__entity;
                        }

                        current_level++;
                    } else {
                        // if there are not neighbors, stop the random walk
                        break;
                    }
                }
                num_random_walks++;
            }
            //print how many neighbors i found for the current entity
            System.out.println("for entity "+entity+" i found "+ myneighbors.size()+" neighbors");
            lenghts_found_neighbors.add(myneighbors.size());
            dictionary_neighbors.put(entity,myneighbors);
            percentage++;
        }

        System.out.println("On average, i found"+ computeAverage(lenghts_found_neighbors) +" neighbors for each entity");

        return dictionary_neighbors;
    }


    /*
    FUNCTION to know in avg how many neighbors have been found for each entity
    input 1: list of numbers of neighbors found for each entity
     */
    public static double computeAverage(List<Integer> numbers) {
        int sum = 0;
        for (int number : numbers) {
            sum += number;
        }
        return (double) sum / numbers.size();
    }

}
