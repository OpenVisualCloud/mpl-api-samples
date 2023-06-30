ID=1
echo ID,filter_type,source_video,in_format,out_format,in_resolution,out_resolution,gpu_time,cpu_time,copy_time,interpol_method,is_static | tee -a result.csv

for file_dir in `ls `
do
   #for csc
   if [[ $file_dir = *log ]] && [[ $file_dir = csc* ]] ;then                                       
     file=$file_dir/1.log
     filter_type=csc
     source_video=`grep "input bitstream" $file | awk -F 'input bitstream' '{print$2}' | awk -F ' ' '{print$1}'| awk -F '/' '{print$3}'`
     in_format=`grep "in_format" $file | awk -F 'in_format' '{print$2}'| awk -F ' ' '{print$1}'`
     out_format=`grep "out_format" $file | awk -F 'out_format' '{print$2}'| awk -F ' ' '{print$1}'`
     in_resolution=`grep "src_size" $file | awk -F 'src_size' '{print$2}' | awk -F ' ' '{print$1}'`
     out_resolution=same_as_input
     gpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$1}' `
     gpu_time=`echo ${gpu_time_temp%??}`
     cpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$2}' `
     cpu_time=`echo ${cpu_time_temp%??}`
     copy_time_temp=`grep "GPU CPU time" $file | awk -F 'copy time' '{print$2}' | awk -F ' ' '{print$1}' `
     copy_time=`echo ${copy_time_temp%??}`
     fps=`grep "frames fps" $file | awk -F '=' '{print$2}'`
     echo $ID,$filter_type,$source_video,$in_format,$out_format,$in_resolution,$out_resolution,$gpu_time,$cpu_time,$copy_time | tee -a result.csv
     ID=$((${ID} + 1))
   fi
   #for resize
   if [[ $file_dir = *log ]] && [[ $file_dir = resize* ]] ;then
     file=$file_dir/1.log
     filter_type=resize
     source_video=`grep "input bitstream" $file | awk -F 'input bitstream' '{print$2}' | awk -F ' ' '{print$1}'| awk -F '/' '{print$3}'`
     in_format=`grep "in_format" $file | awk -F 'in_format' '{print$2}'| awk -F ' ' '{print$1}'`
     out_format=same_as_input
     in_resolution=`grep "src_size" $file | awk -F 'src_size' '{print$2}' | awk -F ' ' '{print$1}'`
     out_resolution=`grep "dst_size" $file | awk -F 'dst_size' '{print$2}' | awk -F ' ' '{print$1}'`
     gpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$1}' `
     gpu_time=`echo ${gpu_time_temp%??}`
     cpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$2}' `
     cpu_time=`echo ${cpu_time_temp%??}`
     copy_time_temp=`grep "GPU CPU time" $file | awk -F 'copy time' '{print$2}' | awk -F ' ' '{print$1}' `
     copy_time=`echo ${copy_time_temp%??}`
     interpol_method=`grep "interp_mtd" $file | awk -F 'interp_mtd' '{print$2}'| awk -F ' ' '{print$1}'`
     echo $ID,$filter_type,$source_video,$in_format,$out_format,$in_resolution,$out_resolution,$gpu_time,$cpu_time,$copy_time,$interpol_method | tee -a result.csv
     ID=$((${ID} + 1))
   fi
   #for composition
   if [[ $file_dir = *log ]] && [[ $file_dir = comp* ]] ;then
     file=$file_dir/1.log
     filter_type=comp
     source_video=`grep "input image" $file | awk -F 'input image' '{print$2}' | awk -F ' ' '{print$1}'| awk -F '/' '{print$3}'`
     in_format=`grep "format" $file | awk -F 'format' '{print$2}'| awk -F ' ' '{print$1}'`
     out_format=same_as_input
     in_resolution=`grep "input image" $file | awk -F 'size' '{print$2}' | awk -F ' ' '{print$1}'`
     out_resolution=same_as_input
     gpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$1}' `
     gpu_time=`echo ${gpu_time_temp%??}`
     cpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$2}' `
     cpu_time=`echo ${cpu_time_temp%??}`
     copy_time_temp=`grep "GPU CPU time" $file | awk -F 'copy time' '{print$2}' | awk -F ' ' '{print$1}' `
     copy_time=`echo ${copy_time_temp%??}`
     echo $ID,$filter_type,$source_video,$in_format,$out_format,$in_resolution,$out_resolution,$gpu_time,$cpu_time,$copy_time | tee -a result.csv
     ID=$((${ID} + 1))
   fi
   #for alphablending
   if [[ $file_dir = *log ]] && [[ $file_dir = alpha* ]] ;then
     file=$file_dir/1.log
     filter_type=alpha
     source_video=`grep "input image" $file | awk -F 'input image' '{print$2}' | awk -F ' ' '{print$1}'| awk -F '/' '{print$3}'`
     in_format=`grep "format" $file | awk -F 'format' '{print$2}'| awk -F ' ' '{print$1}'`
     out_format=same_as_input
     in_resolution=`grep "input image" $file | awk -F 'size' '{print$2}' | awk -F ' ' '{print$1}'`
     out_resolution=same_as_input
     gpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$1}' `
     gpu_time=`echo ${gpu_time_temp%??}`
     cpu_time_temp=`grep "GPU CPU time" $file | awk -F 'CPU time' '{print$2}' | awk -F ' ' '{print$2}' `
     cpu_time=`echo ${cpu_time_temp%??}`
     copy_time_temp=`grep "GPU CPU time" $file | awk -F 'copy time' '{print$2}' | awk -F ' ' '{print$1}' `
     copy_time=`echo ${copy_time_temp%??}`
     interpol_method=" "
     static=yes
     if [ `grep -c "input alpha surface from file" $file` -ne '0' ];then
        static=no
     fi
     echo $ID,$filter_type,$source_video,$in_format,$out_format,$in_resolution,$out_resolution,$gpu_time,$cpu_time,$copy_time,$interpol_method,$static | tee -a result.csv
     ID=$((${ID} + 1))
   fi
done

