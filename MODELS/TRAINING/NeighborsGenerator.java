import javax.swing.plaf.basic.BasicSplitPaneUI;
import java.io.*;
import java.util.*;

import static java.util.Collections.max;

public class NeighborsGenerator {
    private static final Integer K = 3 ;
    private static final Integer omega=5000; //number of random walks of K steps

    //DBPEDDIA 10 000 rw
    // YAGO 3000


    public static void main(String[] args) throws IOException {
        // Apri il file
        File file = new File("C:/Users/elisa/Desktop/ToUpload_COLAB_FILES/allfilestoRun-NELL/train2id_Consistent_withAugmentation.txt");
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

        try (PrintWriter writer = new PrintWriter("C:/Users/elisa/Desktop/ToUpload_COLAB_FILES/allfilestoRun-NELL/file_Neighbors_k3_5000RW.txt")) {
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

    private static Map<Integer, Set<Integer>> get_neighbors(boolean[][] adj_matrix, Set<Integer> all_entities) {
        Map<Integer, Set<Integer>> dictionary_neighbors = new HashMap<>();
        List<Integer> lenghts_found_neighbors=new ArrayList<>();

        Integer percentage=0;
        for (Integer entity : all_entities) {
            System.out.println("Percentage neighbors creation:"+100*(float)percentage/all_entities.size());
            Set<Integer> myneighbors=new HashSet<>();

            int num_random_walks=0;
            int current_entity=entity;
            while(num_random_walks < omega) {

                int current_level=0;
                int last_entity=-1;
                while(current_level<K){
                    //find all the neighbors of the current entity
                    List<Integer> current_neighbors = new ArrayList<>();
                    for (int i = 0; i < adj_matrix[current_entity].length; i++) {
                        if (adj_matrix[current_entity][i]) {
                            if(last_entity!=i)current_neighbors.add(i);
                        }
                    }

                    if(current_neighbors.size()>0) {

                        //choose randomly between them
                        Random random = new Random();
                        int randomIndex = random.nextInt(current_neighbors.size());
                        int chosen__next__entity = current_neighbors.get(randomIndex);

                        //if it's the last step, i found a neighbor of level K, otherwise i keep adding levels of depth
                        if (current_level == K - 1) {
                            myneighbors.add(chosen__next__entity);
                        } else {
                            last_entity = current_entity;
                            current_entity = chosen__next__entity;
                        }

                        current_level++;
                    }
                    else{
                        break;
                    }
                }
                num_random_walks++;
                //System.out.println("per ora ho fatto"+num_random_walks+"random walks");
            }
            System.out.println("for entity "+entity+" i found "+ myneighbors.size()+" neighbors");
            lenghts_found_neighbors.add(myneighbors.size());
            dictionary_neighbors.put(entity,myneighbors);
            percentage++;
        }

        System.out.println("On average, i found"+ computeAverage(lenghts_found_neighbors) +" neighbors for each entity");

        return dictionary_neighbors;
    }
    public static double computeAverage(List<Integer> numbers) {
        int sum = 0;
        for (int number : numbers) {
            sum += number;
        }
        return (double) sum / numbers.size();
    }

}
