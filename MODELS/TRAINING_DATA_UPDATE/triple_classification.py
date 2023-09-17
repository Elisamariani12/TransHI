import numpy as np
import math
import random
import matplotlib.pyplot as plt
import seaborn as sb
import pandas as pd
import random
import argparse


files_src = ""
note = ""
number_iter = -1

relation=[]
entity=[]
inconsistent_wrongly_classified = []


def score(h,t,r):
    res = (entity[h] + relation[r] - entity[t])
    return (-np.linalg.norm(res,1))

def main(files_src, note, number_iter):
    #load embeddings entities
    global entity
    global relation
    global inconsistent_wrongly_classified
    file_in = open(files_src + 'entity2vec'+note+'.vec', 'r')
    for line in file_in:
        x=line.split()
        sub_mat = []
        for val in x:
            sub_mat.append(float(val))
        entity.append(sub_mat)
    entity = np.array(entity)

    #load embeddings relations
    file_in = open(files_src + 'relation2vec'+note+'.vec', 'r')
    for line in file_in:
        x=line.split()
        sub_mat = []
        for val in x:
            sub_mat.append(float(val))
        relation.append(sub_mat)
    relation = np.array(relation)

    #find delta for each relation r: taking as threshold the minimum score in the train set of triples (maximum 1000 considered for efficiency) with that relation
    train=[]
    file_in = open(files_src + 'train2id_Consistent_withAugmentation.txt', 'r')
    first_line = True
    for line in file_in:
        if(first_line):
            first_line = False
        else:
            x=line.split()
            triple=[]
            for val in x:
                triple.append(int(val))
            train.append(triple)
    train = np.array(train)

    acc_f = []
    prec_f = []
    rec_f = []
    fpr_f = []

    acc_o = []
    prec_o = []
    rec_o = []
    fpr_o = []

    for i in range(1):
        num_rel = len(relation)
        delta_r = []
        fail = 0

        #establish a threshold for each relation (and save in delta_r) looking at the scores of 1000 triples of the trainset 
        for i in range(0,num_rel):
            delta_min = -0.75
            min = delta_min
            num = 0
            limit = 1000
            for val in train:
                if(num > limit):
                    break
                if(i == val[2]):
                    num += 1
                    temp = score(val[0], val[1], val[2])
                    if(temp<min): min = temp
            if(num > 0):
                delta_r.append(float(min))
            else:
                fail += 1
                delta_r.append(float(delta_min))

        # (not needed in TransHI but needed if we want to check the accuracy/precision/recall of the classification) - classification of test triples (should be classified as +)
        test_file = files_src + 'test2id_Consistent.txt'
        result=[]
        file_in = open(test_file, 'r')
        first_line = True   #skip first row, it contains the number of triples
        num_test = 0
        for line in file_in:
            if(first_line):
                x = line.split()
                first_line = False
                for val in x:
                    num_test = int(val)
            else:
                x=line.split()
                triple=[]
                for val in x:
                    triple.append(int(val))
                res = score(triple[1], triple[2], triple[3])
                if(res >= delta_r[triple[3]]):
                    result.append(1)  #TRUE POSITIVES
                else:
                    result.append(0)  #FALSE NEGATIVES

        # (needed in TransHI) - classification of inconsistent triples (should be classified as -)
        test_file = files_src + 'InconsistentTriples_'+str(number_iter)+'.txt'
        f_result=[]
        file_in = open(test_file, 'r')
        first_line = False   #skip first row, it contains the number of triples
        num_neg = 0
        for line in file_in:
            if(first_line):
                first_line = False
            else:
                x=line.split()
                triple=[]
                for val in x:
                    triple.append(int(val))
                res = score(triple[1], triple[2], triple[3])
                if(num_neg<num_test):
                    if(res >= delta_r[triple[3]]):
                        f_result.append(0)   #FALSE POSITIVES
                    else:
                        f_result.append(1)   #TRUE NEGATIVES
                num_neg+=1
                if res >= delta_r[triple[3]]:  #I want to save false negatives because they're inconsistent triples predicted as positives
                    inconsistent_wrongly_classified.append(triple)

        print(len(result))
        print(len(f_result))
        # (not needed in TransHI but needed if we want to check the accuracy/precision/recall of the classification) - compute eval metrics
        acc_o.append((result.count(1)+f_result.count(1))/ (len(result) + len(f_result)))
        prec_o.append(result.count(1)/(result.count(1)+f_result.count(0)))
        rec_o.append(result.count(1)/len(result))
        fpr_o.append(f_result.count(0)/(f_result.count(0)+result.count(0)))

        # (not needed in TransHI but needed if we want to check the accuracy/precision/recall of the classification) - write eval metrics results in file
    with open(files_src+"result_classification"+note+".txt", "a") as myfile:
        myfile.write("tc_accuracyL.append(" + str(np.mean(acc_o)) + ")\n")
        myfile.write("tc_precision.append(" + str(np.mean(prec_o)) + ")\n")
        myfile.write("tc_recall.append(" + str(np.mean(rec_o)) + ")\n")
        myfile.write("tc_FPR.append(" + str(np.mean(fpr_o)) + ")\n")

        #  write misclassified triples in file
    with open(files_src+"inconsistent_wrongly_classified.txt", "w") as outfile:
        print("Found "+str(len(inconsistent_wrongly_classified))+"classified in the wrong way");
        outfile.write(str(len(inconsistent_wrongly_classified))+"\n")
        for triple in inconsistent_wrongly_classified:
            outfile.write(" ".join(str(val) for val in triple) + "\n")

    print("Finish")


if __name__ == "__main__":
    my_parser = argparse.ArgumentParser()

    my_parser.add_argument("note", type=str)
    my_parser.add_argument("files_src", type=str)
    my_parser.add_argument("number_iter", type=int)


    args = my_parser.parse_args()

    main(args.files_src, args.note, args.number_iter)

