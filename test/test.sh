for file in `\find . -name '*.mxc'`; do
    echo -n $file :\ 
    ../maxc $file && echo "passed" || echo "failed"
done
