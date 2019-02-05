[openNetVM][onvm]
==

_Please let us know if you use OpenNetVM in your research by [emailing us](mailto:timwood@gwu.edu) or completing this [short survey](https://goo.gl/forms/oxcnGO45Kxq1Zyyi2)._

_Want to get started quickly?_ Try using our NSF CloudLab profile: https://www.cloudlab.us/p/GWCloudLab/onvm


Notes
--

We have updated our DPDK submodule to point to a new version, v18.11.  If you have already cloned this repository, please update your DPDK submodule by running:

```
git submodule sync
git submodule update --init
```

And then rebuild DPDK using the [install guide][install] or running these commands:

```
cd dpdk
make config T=$RTE_TARGET
make T=$RTE_TARGET -j 8
make install T=$RTE_TARGET -j 8
```

See our [release](docs/Releases.md) document for more information.

About
--
openNetVM is a high performance NFV platform based on [DPDK][dpdk] and [Docker][docker] containers.  openNetVM provides a flexible framework for deploying network functions and interconnecting them to build service chains.

openNetVM is an open source version of the NetVM platform described in our [NSDI 2014][nsdi14] and [HotMiddlebox 2016][hotmiddlebox16] papers, released under the [BSD][license] license.  

The [develop][dev] branch tracks experimental builds (active development) whereas the [master][mast] branch tracks verified stable releases.  Please read our [releases][rels] document for more information about our releases and release cycle.

You can find information about research projects building on [OpenNetVM][onvm] at the [UCR/GW SDNFV project site][sdnfv]. OpenNetVM is supported in part by NSF grants CNS-1422362 and CNS-1522546. 

Installing
--
To install openNetVM, please see the [openNetVM Installation][install] guide for a thorough walkthrough.

Using openNetVM
--
openNetVM comes with several sample network functions.  To get started with some examples, please see the [Example Uses][examples] guide

Creating NFs
--
The [NF Development][nfs] guide will provide what you need to start creating your own NFs.

Dockerize NFs
--
NFs can be run inside docker containers, with the NF being automatically or hand started. For more informations, see our [Docker guide][docker-nf].

TCP Stack
--
openNetVM can run mTCP applications as NFs. For more information, visit [mTCP][mtcp].

Citing OpenNetVM
--
If you use OpenNetVM in your work, please cite our paper:
```
@inproceedings{zhang_opennetvm:_2016,
	title = {{OpenNetVM}: {A} {Platform} for {High} {Performance} {Network} {Service} {Chains}},
	booktitle = {Proceedings of the 2016 {ACM} {SIGCOMM} {Workshop} on {Hot} {Topics} in {Middleboxes} and {Network} {Function} {Virtualization}},
	publisher = {ACM},
	author = {Zhang, Wei and Liu, Guyue and Zhang, Wenhui and Shah, Neel and Lopreiato, Phillip and Todeschi, Gregoire and Ramakrishnan, K.K. and Wood, Timothy},
	month = aug,
	year = {2016},
}
```

_Please let us know if you use OpenNetVM in your research by [emailing us](mailto:timwood@gwu.edu) or completing this [short survey](https://goo.gl/forms/oxcnGO45Kxq1Zyyi2)._





[onvm]: http://sdnfv.github.io/onvm/
[sdnfv]: http://sdnfv.github.io/
[license]: LICENSE
[dpdk]: http://dpdk.org
[docker]: https://www.docker.com/
[nsdi14]: http://faculty.cs.gwu.edu/timwood/papers/14-NSDI-netvm.pdf
[hotmiddlebox16]: http://faculty.cs.gwu.edu/timwood/papers/16-HotMiddlebox-onvm.pdf
[install]: docs/Install.md
[examples]: docs/Examples.md
[nfs]: docs/NF_Dev.md
[docker-nf]: docs/Docker.md
[dev]: https://github.com/sdnfv/openNetVM/tree/develop
[mast]: https://github.com/sdnfv/openNetVM/tree/master
[rels]: docs/Releases.md
[mtcp]: https://github.com/eunyoung14/mtcp
