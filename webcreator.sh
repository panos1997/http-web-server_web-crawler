#! /bin/bash

root_dir=$1                    #root directory given as a parameter from command line
text_file=$2			#text_file       //    //      //         //
w=$3
p=$4

if [ ! -f $text_file ]; then               #CHECKS
	echo "File not found!"
	exit 1                     #text file does not exist
fi

if [ ! -d $root_dir ]; then
	echo "dir not found!"
	exit 1                     #root dir does not exist
fi

if [ ! -z "$(ls -A $root_dir)" ]; then
	echo "Warning: directory is full, purging ..."
	rm -rf root_dir/*
fi

if [[ ! $w =~ ^-?[0-9]+$ ]]; then             #checking if w is an integer
	echo 'w is not an integer'
	exit 1
fi

if [[ ! $p =~ ^-?[0-9]+$ ]]; then            #checking if p is an integer
	echo 'p is not an integer'
	exit 1
fi

lines_num=$(awk 'END{print NR}' $text_file)                #number of lines of text_file

if [ $lines_num -lt 10000 ]; then                    #if it has less than 10.0000 lines, exit
	echo 'file has a small number of lines'
	exit 2
fi

counter=0
while [  $counter -lt $w ]; do                     #create 'w' directories (sites) inside root directory
	var=$(printf 'site%d' "$counter")
	echo "creating website " $counter ...
	let counter=$counter+1
	mkdir -p $root_dir/$var;
done

counter=0
total_counter=0
while [  $counter -lt $w ]; do                          #for every directory create 'p' pages
	counter2=0
	var=$(printf 'site%d' "$counter")               #var contains siteX
	while [  $counter2 -lt $p ]; do
		i=$counter
		j=$(((RANDOM % 100000)+1))
		var2=$(printf 'page%d_%d.html' "$i" "$j")           #var2 contains pagei_j.html
		touch $root_dir/$var/$var2							#create sitex/pagei_j inside root_dir
		total_pages[total_counter]=$var2      		      #after the loop it will contain all of the pages in all of the sites
		sites_of_pages[total_counter]=$var       #contains the site that the current page belongs
		let total_counter=$total_counter+1
		let counter2=$counter2+1
	done
	let counter=$counter+1
done

#so far we have created the arrays: total_pages[],sites_of_pages[]
	#total_pages[] contains all of the pages that have been created
	#sites_of_pages[] contains for every page,the site that it belongs



f_num=$(expr $p / 2)
let f_num=$f_num+1                         #synolo f istoselidwn mesa sto idio site ths arxikhs
q_num=$(expr $w / 2)
let q_num=$q_num+1						#synolo q istoselidwn apo diaforetika sites

total_internal_links[p]=0				#stores ALL the internal links
total=$(expr $p \* $w)
total_external_links[total]=0			#stores ALL the external links
f_counter=0
q_counter=0
let total_counter=$total_counter-1

#thelw na gemisw KATHE istoselida me periexomeno ('total_counter' istoselides)
for i in $(seq 0 $total_counter); do    			#gia kathe istoselida
	k=$(((RANDOM % $lines_num)+1))  #create k,m (for putting html content into pages)
	m=$(((RANDOM % 1000)+1000))
	cur_page=${total_pages[i]}                #the page we EXAMINE
	cur_site=${sites_of_pages[i]}				#the site the page belongs to
							#(this is for internal links)
	for page in "$root_dir/$cur_site"/*; do       #gia kathe page tou site pou anhkei h istoselida pou exetazoume
		page=$(basename $page)	          #get only name of page (without the full path)
		if [ "$page" != "$cur_page" ]; then     #an to page einai != apo to page pou exetazoume
			f[f_counter]=../$cur_site/$page             #array f contains the internal links
			let f_counter=$f_counter+1
		fi
	done
							#(this is for external links)
	for j in $(seq 0 $p $total_counter); do            #gia kathe site (me vhma 'p')
		if [ "${sites_of_pages[j]}" != "$cur_site" ]; then   #an einai diaforetiko tou trexontos site
			for page in "$root_dir/${sites_of_pages[j]}"/*; do  #gia kathe page tou site
				page=$(basename $page)	          #get only name of page (without the full path)
				q[q_counter]=/${sites_of_pages[j]}/$page
				let q_counter=$q_counter+1
			done
		fi
	done

#at this point we have created two arrays (f[] and q[])
	#f[] contains ALL the possible internal links (for the current page)
	#q[] contains ALL the possible external links	(       //         )

	let bound=$f_counter-1
	nums=($(shuf -i 0-$bound -n $f_num)) 		 #generate 'f_num' random numbers in range (0,f_counter-1)
	for i in $(seq 1 $f_num); do               #create f2[] from f[]    (f2 is smaller than f)
		let l=$i-1					#choosing f_num INTERNAL LINKS (all of them are 'f_counter' links)
		f2[l]=${f[${nums[l]}]}
	done

	let bound=$q_counter-1
	nums=($(shuf -i 0-$bound -n $q_num))       #generate 'q_num' random numbers in range (0,q_counter-1)
	for i in $(seq 1 $q_num); do               #create q2[] from q[]    (q2 is smaller than q)
		let l=$i-1				# choosing q_num EXTERNAL LINKS (all of them are 'q_counter' links)
		q2[l]=${q[${nums[l]}]}
	done

#at this point we have created another two arrays (f2[] and q2[])
	#f2[] contains SOME of the internal links
	#q2[] contains SOME if the external links


	echo "<!DOCTYPE html>" >> $root_dir/$cur_site/$cur_page                   #writing html headers in page (at beginning of file)
	echo "<html>" >> $root_dir/$cur_site/$cur_page
	echo -e "\t<body>" >> $root_dir/$cur_site/$cur_page
	let total=$f_num+$q_num										#all the work is here
	lines_to_copy=$(expr $m / $total)     					#number of lines for each packet to copy
	let until_line=$k+$lines_to_copy					#at which line the packet ends
	count=0
	count2=0
	echo creating page $root_dir/$cur_site/$cur_page with $m lines, starting at line $k
	while [ $count -lt $total ]; do         #vazw sto page ta packeta me tis grammes kai ta links
		if [ $count -lt $f_num ]; then          #vazw ta internal links
			awk 'NR >= '$k' && NR <= '$until_line'' $text_file >> $root_dir/$cur_site/$cur_page  #write the packet of lines to the page
			echo adding link to $root_dir/$cur_site/$cur_page
			#echo 	f2=========== ${f2[count]}
			echo  "<a href="${f2[count]}">link_text</a>" >> $root_dir/$cur_site/$cur_page
			let k=$k+$lines_to_copy
			let until_line=$k+$lines_to_copy	#at which line the packet ends
		else									#vazw ta external links
			awk 'NR >= '$k' && NR <= '$until_line'' $text_file >> $root_dir/$cur_site/$cur_page  #write the packet of lines to the page
			echo adding link to $root_dir/$cur_site/$cur_page
			echo  "<a href="..${q2[count2]}">link_text</a>" >> $root_dir/$cur_site/$cur_page
		#	echo 	q2=========== ${q2[count2]}
			let k=$k+$lines_to_copy
			let until_line=$k+$lines_to_copy	#at which line the packet ends
			let count2=$count2+1
		fi
		let count=$count+1
	done



	echo -e "\t</body>" >> $root_dir/$cur_site/$cur_page                      #writing closing html headers in page
	echo "</html>" >> $root_dir/$cur_site/$cur_page
	echo >> $root_dir/$cur_site/$cur_page
	f_counter=0
	q_counter=0
done
echo done..
exit 0
