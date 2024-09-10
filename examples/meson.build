# List of all the onvm example apps
onvm_examples = [
    'aes_decrypt',
    'aes_encrypt',
    'arp_response',
    'basic_monitor',
    'bridge',
    'fair_queue',
    'firewall',
    #   'flow_table',
    'flow_tracker',
    'l2fwd',
    'l3fwd',
    'load_balancer',
    'load_generator',
    #    'ndpi_stats',
    'nf_router',
    #    'NFD',
    'payload_scan',
    'scaling_example',
    'simple_forward',
    'simple_fwd_tb',
    'skeleton',
    'speed_tester',
    'test_flow_dir',
    'test_messaging'
]

foreach example : onvm_examples
    name = 'onvm_' + example
    sources = []
    includes = [include_directories(example), onvm_includes]

    subdir(example)

    executable(
        name,
        sources,
        include_directories: includes,
        dependencies: [onvm_dpdk_dep, onvm_nflib_dep],
        install: true,
        install_tag: 'onvm',
        install_dir: onvm_output_dir + '/nf/' + example
    )
endforeach
