cd $1
opt -dot-cfg *.bc
cd ..
rm $1/*.pdf
for file in $1/*.dot; do
	dot -Tpdf $file > ${file}.pdf
done
