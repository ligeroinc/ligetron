#! /usr/bin/bash

prover=./prover
verifier=./verifier
prefix=../wasm/

bench() {
	program=$prefix/$1.wasm
	pfile="$program_$1_$2_$3.txt"
	input1=$(openssl rand -hex $(($2 / 2)))
	input2=$(openssl rand -hex $(($3 / 2)))
	echo "OMP threads: $OMP_NUM_THREADS"  > $pfile
	echo ''                              >> $pfile
	echo "input1: $input1"               >> $pfile
	echo "input2: $input2"               >> $pfile

	echo "Running prover for $program of input $1 $2"
	/usr/bin/time -v $prover $program $4 $input1 $input2 &>> $pfile

	echo '                                                            ' >> $pfile
	echo '============================================================' >> $pfile
	echo '                                                            ' >> $pfile
	
	echo "Running verifier for $program of input $1 $2"
	/usr/bin/time -v $verifier $program $4 $input1 $input2 &>> $pfile
}

bench edit 4 4 333
bench edit 4 8 333
bench edit 8 8 333
bench edit 8 16 840
bench edit 16 16 840
bench edit 16 32 840
bench edit 32 32 1861
bench edit 32 64 1861
bench edit 64 64 3908
bench edit 64 128 3908
bench edit 128 128 8003
bench edit 128 256 8003
bench edit 256 256 16195
bench edit 256 512 16195
bench edit 512 512  32579     # 64M
# bench edit 512 1024 32579   # 125M
# bench edit 1024 1024 65347  # 250M
# bench edit 1024 2048 65347  # 500M
# bench edit 2048 2048 65347  # 1B
