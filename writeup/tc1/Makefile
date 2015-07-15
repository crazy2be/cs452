all: README.tex
	pdflatex -shell-escape tc1.tex

README.tex: Makefile
	cat ../../README.md \
		| sed -e '1,/BEGIN LaTeX INCLUDED/d' \
		| sed -e '/END LaTeX INCLUDED/,$d' \
		| pandoc -f markdown -t latex > README.tex
