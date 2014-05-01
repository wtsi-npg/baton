
EXTRA_DIST = conf.py index.rst

SPHINXOPTS = -d _build/doctrees

man1_MANS = baton.1

baton.1: index.rst
	$(SPHINXBUILD) -b man $(SPHINXOPTS) $(srcdir) $(srcdir)

html-local:
	@mkdir -p _static
	@mkdir -p _build/doctrees
	$(SPHINXBUILD) -b html $(SPHINXOPTS) $(srcdir) _build/html

install-html-local: html
	@mkdir -p $(docdir)
	@cp -r _build/html $(docdir)

uninstall-local:
	@rm -rf $(docdir)

clean-local:
	@rm -rf _build
