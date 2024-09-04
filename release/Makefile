TAG=$(shell git describe --always)
ifeq ($(GITHUB_ACTIONS),true)
DOCKER_REGISTRY?=ghcr.io
DOCKER_USER?=$(GITHUB_REPOSITORY_OWNER)
else
DOCKER_REGISTRY?=docker.io
DOCKER_USER?=wsinpg
endif

NOW=$(shell date --utc --iso-8601=seconds)

DOCKER_PREFIX?=$(DOCKER_REGISTRY)/$(DOCKER_USER)
DOCKER_ARGS ?= --platform linux/amd64 --progress=plain --rm
.PHONY: clean push

image_names += ub-16.04-baton-irods-4.2.7
image_names += ub-18.04-baton-irods-4.2.11
image_names += ub-18.04-baton-irods-4.2.12
image_names += ub-22.04-baton-irods-4.3.2
image_names += ub-22.04-baton-irods-4.3.3

git_url=$(shell git remote get-url origin)
git_commit=$(shell git log --pretty=format:'%H' -n 1)

images := $(addsuffix .$(TAG), $(image_names))
remote := $(addsuffix .$(TAG).pushed, $(image_names))

all: $(images)

push: $(remote)

ub-16.04-baton-irods-4.2.7.$(TAG): ubuntu/16.04/Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION="4.2.7" \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS 4.2.7 baton client, Ubuntu 16.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--label org.opencontainers.image.vendor=npg.sanger.ac.uk \
	--tag $(DOCKER_PREFIX)/ub-16.04-baton-irods-4.2.7:latest \
	--tag $(DOCKER_PREFIX)/ub-16.04-baton-irods-4.2.7:$(TAG) --file $< .
	touch $@

ub-18.04-baton-irods-4.2.11.$(TAG): ubuntu/18.04/Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION="4.2.11" \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS 4.2.11 baton client, Ubuntu 18.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--label org.opencontainers.image.vendor=npg.sanger.ac.uk \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-4.2.11:latest \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-4.2.11:$(TAG) --file $< .
	touch $@

ub-18.04-baton-irods-4.2.12.$(TAG): ubuntu/18.04/Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION="4.2.12" \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS 4.2.12 baton client, Ubuntu 18.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--label org.opencontainers.image.vendor=npg.sanger.ac.uk \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-4.2.12:latest \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-4.2.12:$(TAG) --file $< .
	touch $@

ub-22.04-baton-irods-4.3.2.$(TAG): ubuntu/22.04/Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION="4.3.2" \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS 4.3.2 baton client, Ubuntu 22.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--label org.opencontainers.image.vendor=npg.sanger.ac.uk \
	--tag $(DOCKER_PREFIX)/ub-22.04-baton-irods-4.3.2:latest \
	--tag $(DOCKER_PREFIX)/ub-22.04-baton-irods-4.3.2:$(TAG) --file $< .
	touch $@

ub-22.04-baton-irods-4.3.3.$(TAG): ubuntu/22.04/Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION="4.3.3" \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS 4.3.3 baton client, Ubuntu 22.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--label org.opencontainers.image.vendor=npg.sanger.ac.uk \
	--tag $(DOCKER_PREFIX)/ub-22.04-baton-irods-4.3.3:latest \
	--tag $(DOCKER_PREFIX)/ub-22.04-baton-irods-4.3.3:$(TAG) --file $< .
	touch $@

%.$(TAG).pushed: %.$(TAG)
	docker push $(DOCKER_PREFIX)/$*:$(TAG)
	docker push $(DOCKER_PREFIX)/$*:latest
	touch $@

clean:
	rm -f $(foreach image_name,$(image_names), $(image_name)*)
