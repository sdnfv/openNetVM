sources = files(
    'onvm_config_common.c',
    'onvm_flow_dir.c',
    'onvm_flow_table.c',
    'onvm_nflib.c',
    'onvm_pkt_common.c',
    'onvm_pkt_helper.c',
    'onvm_sc_common.c',
    'onvm_sc_mgr.c',
    'onvm_threading.c'
)

libonvm_includes = include_directories('.')

libonvm_deps = [
    onvm_dpdk_dep,
    onvm_thread_dep,
    libonvmhelper_dep
]

libonvm = library(
    'libonvm', 
    sources, 
    include_directories : libonvm_includes,
    dependencies : libonvm_deps,
    pic: true,
    install: true,
    install_tag: 'onvm')

libonvm_dep = declare_dependency(
    link_with: libonvm,
    dependencies: libonvm_deps)

onvm_nflib_dep += libonvm_dep