#!/bin/bash -x
source benchmark.config

#for resize
if [ "$resize" == "1" ]; then
l1=${#resize_format[@]}
for format in $(seq $l1)
do
    l2=${#resize_in_size[@]}
    for size in $(seq $l2)
    do
        l3=${#interp_method[@]}
        for method in $(seq $l3)
        do
            index=${resize_format[$format-1]}_${resize_in_size[$size-1]}
            input_file=${resize_format_size_inputfile[$index]}
            log_dir=resize_${resize_format[$format-1]}_${interp_method[$method-1]}_${resize_in_size[$size-1]}_${resize_out_size[$size-1]}_log
            mkdir $log_dir
            for i in $(seq $multi_process_number)
            do 
                each_video_log=$log_dir/$i.log
                ../build/resize -pre_read -in_size ${resize_in_size[$size-1]} -out_size ${resize_out_size[$size-1]} -frame $frame_value -in_format ${resize_format[$format-1]} -interp_mtd ${interp_method[$method-1]}  -i $clips_folder/$input_file | tee $each_video_log &
            done
            wait
        done
    done
done
fi

#for csc
if [ "$csc" == "1" ]; then
l1=${#csc_in_format[@]}
for format in $(seq $l1)
do   
    l2=${#csc_size[@]}
    for size in $(seq $l2)
    do
        index=${csc_in_format[$format-1]}_${csc_size[$size-1]}
        input_file=${csc_format_size_inputfile[$index]}
        log_dir=csc_${csc_in_format[$format-1]}_${csc_out_format[$format-1]}_${csc_size[$size-1]}_log
        mkdir $log_dir
        for i in $(seq $multi_process_number)
        do
            each_video_log=$log_dir/$i.log
            ../build/csc -pre_read -size ${csc_size[$size-1]} -frame $frame_value -in_format ${csc_in_format[$format-1]} -out_format ${csc_out_format[$format-1]} -i $clips_folder/$input_file | tee $each_video_log &
        done
        wait
    done
done
fi

#for composition
if [ "$composition" == "1" ]; then
l1=${#comp_format[@]}
for format in $(seq $l1)
do
    comp_index=${comp_format[$format-1]}_${comp_size[$format-1]}
    comp1_index=${comp_format[$format-1]}_${comp1_size[$format-1]}
    input_file=${comp_format_size_inputfile[$comp_index]}
    input1_file=${comp_format_size_inputfile[$comp1_index]}
    log_dir=comp_${comp_format[$format-1]}_log
    mkdir $log_dir
    for i in $(seq $multi_process_number)
    do
        each_video_log=$log_dir/$i.log
        ../build/composition -pre_read -in_format ${comp_format[$format-1]}  -frame $frame_value -size ${comp_size[$format-1]} -compsize ${comp1_size[$format-1]} -offset $comp_offset -compfile $clips_folder/$input1_file -i $clips_folder/$input_file | tee $each_video_log &
    done
    wait
done
fi

#for alphablending
if [ "$alphablending" == "1" ]; then
l1=${#alpha_format[@]}
for format in $(seq $l1)
do
    alpha_index=${alpha_format[$format-1]}_${alpha_size[$format-1]}
    alpha1_index=${alpha_format[$format-1]}_${alpha1_size[$format-1]}
    input_file=${alpha_format_size_inputfile[$alpha_index]}
    input1_file=${alpha_format_size_inputfile[$alpha1_index]}
    static_log_dir=alpha_${alpha_format[$format-1]}_static_log
    bin_log_dir=alpha_${alpha_format[$format-1]}_bin_log
    mkdir $static_log_dir
    mkdir $bin_log_dir
    for i in $(seq $multi_process_number)
    do
        static_each_video_log=$static_log_dir/$i.log
        ../build/alphablending -pre_read -in_format ${alpha_format[$format-1]} -frame $frame_value -size ${alpha_size[$format-1]} -static_alpha $static_alpha -compsize ${alpha1_size[$format-1]} -offset $alpha_offset -compfile $clips_folder/$input1_file -i $clips_folder/$input_file | tee $static_each_video_log &
    done
    wait
    for j in $(seq $multi_process_number)
    do
        bin_each_video_log=$bin_log_dir/$j.log
        ../build/alphablending -pre_read -in_format ${alpha_format[$format-1]} -frame $frame_value -size ${alpha_size[$format-1]} -compsize ${alpha1_size[$format-1]} -offset $alpha_offset -compfile $clips_folder/$input1_file -alphafile $clips_folder/$alphafile -i $clips_folder/$input_file | tee $bin_each_video_log &
    done
    wait
done
fi
