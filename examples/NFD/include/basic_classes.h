/**********************************************************************************
                               NFD project
   A C++ based NF developing framework designed by Wenfei's group
   from IIIS, Tsinghua University, China.
******************************************************************************/

/************************************************************************************
* Filename:   basic_classes.h
* Author:     Hongyi Huang(hhy17 AT mails.tsinghua.edu.cn), Bangwen Deng, Wenfei Wu
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    This file is a supprot file for NFD project, defining the types maybe
              used in NFD NF.
*************************************************************************************/

#include <stdarg.h>
#include <stdint.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pcap.h>

using namespace std;
class IP;
typedef unordered_set<IP> ipset;
enum header { Iplen = 0, Sport = 1, Dport = 2, Tcp = 3, Udp = 4, Sip = 10, Dip = 11, Tag = 20 };

#define ERROR_HANDLE(x) std::cout << "Error Information: " << x << endl;
std::vector<std::string>
split(const std::string& text, char sep);

template <typename T>
T
union_set(T& s1, T& s2) {
        T result = s1;
        result.insert(s2.cbegin(), s2.cend());
        return result;
}

template <class T>
unordered_set<T>&
create_set(unordered_set<T>& ns, int count, ...) {
        va_list ap;
        va_start(ap, count);
        for (int i = 0; i < count; i++) {
                T item = *((T*)va_arg(ap, void*));
                ns.insert(item);
        }
        return ns;
}

template <typename A, typename B>
unordered_map<A, B>&
create_map(unordered_map<A, B>& ns, int count, ...) {
        va_list ap;
        va_start(ap, count);
        for (int i = 0; i < count; i = i + 2) {
                A key = *((A*)va_arg(ap, void*));
                B value = *((B*)va_arg(ap, void*));
                ns[key] = value;
        }
        return ns;
}

class F_Type {
       public:
        static unordered_map<string, int> MAP;
        static unordered_map<string, int> MAP2;
        static void
        init() {
                /* TYPE  int  == 1  */
                MAP["dport"] = 1;
                MAP["sport"] = 1;

                /* TYPE  IP   == 2  */
                MAP["sip"] = 2;
                MAP["dip"] = 2;

                MAP2["sip"] = 2;
                MAP2["dip"] = 3;

                /* TYPE others == 3  */
                MAP["tag"] = 3;
                MAP2["tag"] = 1;
        }

        static int
        type_id(string& field, int* ret2) {
                int ret1;
                auto res = MAP.find(field);
                if (res != MAP.end()) {
                        ret1 = res->second;
                } else {
                        std::cout << "type_id SEARCHING " << field << " ERROR 12390" << endl;
                        ret1 = 0;
                }
                auto res2 = MAP2.find(field);
                if (res2 != MAP2.end()) {
                        *ret2 = res2->second;
                } else {
                        std::cout << "type_id SEARCHING " << field << " ERROR 12390" << endl;
                        *ret2 = 0;
                }
                return ret1;
        }
};

class Flow {
        u_char* pkt;
        void* q = new string("error");
        unordered_map<string, void*> field_value;
        friend struct FlowCmp;

       public:
        void* headers[30];
        // Flow(int count, ...);
        Flow(){};
        Flow(int* tag);
        Flow(u_char* pkt, int totallength);
        void*& operator[](const string& field);
        int
        matches(const string& field, const void* p);
        void
        clean();
};

/*IP class for reserving IP*/
class IP {
       private:
       public:
        uint32_t ip;
        uint32_t mask;
        IP(const string& raw_ip, int raw_mask);
        IP(int ip, int mask);
        IP(const string& raw_ip);
        IP() {
        }
        char*
        showAddr();
        // bool contains(const IP& ip2) const;
        // bool operator>=(const IP& other);
        bool
        operator<=(const IP& other);
        bool
        operator==(const IP& other) const;
        bool
        operator!=(const IP& other);
};

class Tuple {
       private:
       public:
        vector<int> ints;
        vector<IP> ips;
        Tuple(const vector<int>& ins, const vector<IP>& is) {
                this->ints = ins;
                this->ips = is;
        }
};

template <typename T>
class State {
       private:
        vector<header> keywords;
        unordered_map<Tuple, T> states;
        T gl_state;
        bool global = false;

       public:
        T init;
        int
        getSize() {
                if (this->global == true)
                        return 1;
                return this->states.size();
        }
        Tuple
        create_tuple(Flow& f) {
                vector<int> v_int;
                vector<IP> v_ip;
                auto it = this->keywords.begin();
                for (; it != this->keywords.end(); it++) {
                        if (*it < 10) {
                                v_int.push_back(*((int*)f.headers[*it]));
                                continue;
                        } else if (*it >= 10 && *it < 20) {
                                v_ip.push_back(*((IP*)f.headers[*it]));
                                continue;
                        } else {
                                continue;
                        }
                }
                Tuple tp(v_int, v_ip);
                return tp;
        }
        /*Initial*/
        /* count is number of fields/keywords to distinct two state instances*/
        State(T ini, string input) {
                this->init = ini;
                std::vector<string> fields = split(input, '&');
                std::vector<string>::iterator it = fields.begin();
                for (; it != fields.end(); it++) {
                        header h1;
                        if (*it == "iplen") {
                                h1 = Iplen;
                        } else if (*it == "sport") {
                                h1 = Sport;
                        } else if (*it == "dport") {
                                h1 = Dport;
                        } else if (*it == "sip") {
                                h1 = Sip;
                        } else if (*it == "dip") {
                                h1 = Dip;
                        } else if (*it == "tag") {
                                h1 = Tag;
                        }
                        this->keywords.push_back(h1);
                }
                if (input == "")
                        this->global = true;
                return;
        }
        /* Globally shared state */
        State(T ini) {
                this->gl_state = ini;
                this->global = true;
                return;
        }

        /* [] return states of type T belonging to f*/
        T& operator[](Flow& f) {
                /*
                   1. new a Tuple
                   2. see if f is among keys
                   a. yes, push_back a new pair
                   b. no, create new pair, initialize it
                 */
                if (this->global == false) {
                        auto tp = create_tuple(f);
                        auto it = states.find(tp);
                        if (it == states.end()) {
                                // not exist in keys of map
                                this->states[tp] = init;
                        }
                        return this->states[tp];
                } else {
                        return this->gl_state;
                }
        }
};

namespace std {
template <>
struct hash<IP> {
        std::size_t
        operator()(const IP& ip) const {
                using std::hash;
                using std::size_t;

                // Compute individual hash values for first,
                // second and third and combine them using XOR
                // and bit shifting:

                return ((hash<int>()(ip.ip) ^ (hash<int>()(ip.mask) << 1)) >> 1);
        }
};
template <>
struct hash<vector<int>> {
        std::size_t
        operator()(const vector<int> ins) const {
                using std::hash;
                using std::size_t;
                size_t ret = 1;

                auto lp = ins.begin();
                for (; lp != ins.end(); lp++) {
                        ret = ret ^ (hash<int>()(*lp) << 1) >> 1;
                }
                return ret;
        }
};
template <>
struct hash<vector<IP>> {
        std::size_t
        operator()(const vector<IP> ips) const {
                using std::hash;
                using std::size_t;
                size_t ret = 1;

                auto lp = ips.begin();
                for (; lp != ips.end(); lp++) {
                        ret = ret ^ ((hash<int>()((*lp).ip) ^ (hash<int>()((*lp).mask) << 1)) >> 1);
                }
                return ret;
        }
};

template <>
struct equal_to<IP> {
        bool
        operator()(const IP& lhs, const IP& rhs) const {
                return (lhs.ip == rhs.ip) && (lhs.mask == rhs.mask);
        }
};
template <>
struct hash<Tuple> {
        std::size_t
        operator()(const Tuple& tp) const {
                using std::hash;
                using std::size_t;

                // Compute individual hash values for first,
                // second and third and combine them using XOR
                // and bit shifting:

                return ((hash<vector<int>>()(tp.ints) ^ (hash<vector<IP>>()(tp.ips) << 1)) >> 1);
        }
};

template <>
struct equal_to<Tuple> {
        bool
        operator()(const Tuple& lhs, const Tuple& rhs) const {
                if (lhs.ints.size() == rhs.ints.size()) {
                        auto lp = lhs.ints.begin();
                        auto rp = rhs.ints.begin();
                        for (; lp != lhs.ints.end(); lp++, rp++) {
                                if (*lp != *rp) {
                                        return false;
                                }
                        }
                } else
                        return false;
                if (lhs.ips.size() == rhs.ips.size()) {
                        auto lp = lhs.ips.begin();
                        auto rp = rhs.ips.begin();
                        for (; lp != lhs.ips.end(); lp++, rp++) {
                                if (!(*lp == *rp)) {
                                        return false;
                                }
                        }
                } else
                        return false;
                return true;
        }
};
}  // namespace std
