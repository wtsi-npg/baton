
EXTRA_DIST = conf.py index.rst

SPHINXOPTS = -d html/doctrees

dist_man_MANS = baton.1

baton.1: index.rst
	$(SPHINXBUILD) -b man $(SPHINXOPTS) $(srcdir) $(srcdir)

html-local:
	@mkdir -p html/_static
	@mkdir -p html/doctrees
	$(SPHINXBUILD) -b html $(SPHINXOPTS) $(srcdir) html

install-html-local: html
	@mkdir -p $(docdir)
	@cp -r html $(docdir)

uninstall-local:
	@rm -rf $(docdir)

clean-local:
	@rm -rf html
