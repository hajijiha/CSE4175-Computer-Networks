#include "netsim2.h"

#include <vector>
#include <set>
#include <algorithm>
#include <cstdint>
#include <climits>

#ifndef USE_COST
#define USE_COST 0
#endif

struct LSARecord {
    bool valid;
    uint8_t seq;
    std::vector<std::pair<int,int> > nbrs;
    LSARecord() : valid(false), seq(0) {}
};

struct RouterState {
    int my_id;
    int num_nodes;
    uint8_t my_seq;
    bool dirty;
    std::vector<LSARecord> db;
    std::vector<int> next_hop;
    std::set<int> to_sync;
};

static inline bool seq_greater(uint8_t a, uint8_t b) {
    return a != b && (uint8_t)(a - b) < 0x80u;
}

static void encode_records(RouterState* s, const std::vector<int>& origins,
                           std::vector<uint8_t>& out) {
    out.clear();
    for (size_t i = 0; i < origins.size(); ++i) {
        const LSARecord& r = s->db[origins[i]];
        out.push_back((uint8_t)origins[i]);
        out.push_back((uint8_t)r.seq);
        out.push_back((uint8_t)r.nbrs.size());
        for (size_t j = 0; j < r.nbrs.size(); ++j) {
            out.push_back((uint8_t)r.nbrs[j].first);
#if USE_COST
            out.push_back((uint8_t)r.nbrs[j].second);
#endif
        }
    }
}

static int record_size(RouterState* s, int o) {
    int per = (int)s->db[o].nbrs.size();
#if USE_COST
    per *= 2;
#endif
    return 3 + per;
}

static void send_full_db(RouterState* s, int nb) {
#ifndef SYNC_CAP
#define SYNC_CAP 65000
#endif
    const int CAP = SYNC_CAP;
    std::vector<int> chunk;
    int sz = 0;
    for (int i = 0; i < s->num_nodes; ++i) {
        if (!s->db[i].valid) continue;
        int rs = record_size(s, i);
        if (!chunk.empty() && sz + rs > CAP) {
            std::vector<uint8_t> buf;
            encode_records(s, chunk, buf);
            send_control(nb, buf.data(), (int)buf.size());
            chunk.clear();
            sz = 0;
        }
        chunk.push_back(i);
        sz += rs;
    }
    if (!chunk.empty()) {
        std::vector<uint8_t> buf;
        encode_records(s, chunk, buf);
        send_control(nb, buf.data(), (int)buf.size());
    }
}

static void flood(RouterState* s, const std::vector<uint8_t>& buf,
                  int exclude1, int exclude2) {
    if (buf.empty()) return;
    const std::vector<std::pair<int,int> >& me = s->db[s->my_id].nbrs;
    for (size_t i = 0; i < me.size(); ++i) {
        int nb = me[i].first;
        if (nb == exclude1 || nb == exclude2) continue;
        send_control(nb, buf.data(), (int)buf.size());
    }
}

static bool has_neighbor(const std::vector<std::pair<int,int> >& v, int id) {
    for (size_t i = 0; i < v.size(); ++i) if (v[i].first == id) return true;
    return false;
}

static void compute(RouterState* s, std::vector<int>& out, bool relaxed) {
    int N = s->num_nodes;
    std::vector<long long> dist(N, LLONG_MAX);
    std::vector<int> fh(N, -1);
    std::vector<char> done(N, 0);
    dist[s->my_id] = 0;
    for (int it = 0; it < N; ++it) {
        int u = -1;
        long long best = LLONG_MAX;
        for (int i = 0; i < N; ++i) {
            if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
        }
        if (u < 0) break;
        done[u] = 1;
        if (!s->db[u].valid) continue;
        const std::vector<std::pair<int,int> >& adj = s->db[u].nbrs;
        for (size_t k = 0; k < adj.size(); ++k) {
            int v = adj[k].first;
            int c = adj[k].second;
            if (v < 0 || v >= N) continue;
            if (u != s->my_id && !relaxed) {
                if (!s->db[v].valid) continue;
                if (!has_neighbor(s->db[v].nbrs, u)) continue;
            }
            long long nd = dist[u] + c;
            int nfh = (u == s->my_id) ? v : fh[u];
            if (nd < dist[v]) {
                dist[v] = nd;
                fh[v] = nfh;
            } else if (nd == dist[v] && nfh >= 0 && (fh[v] < 0 || nfh < fh[v])) {
                fh[v] = nfh;
            }
        }
    }
    out.assign(N, -1);
    for (int i = 0; i < N; ++i) out[i] = fh[i];
    out[s->my_id] = -1;
}

static void recompute(RouterState* s) {
    compute(s, s->next_hop, false);
}

extern "C" RouterState* router_init(int my_id, int num_nodes,
                                    const int* neighbor_ids,
                                    const int* link_costs,
                                    int num_neighbors) {
    RouterState* s = new RouterState();
    s->my_id = my_id;
    s->num_nodes = num_nodes;
    s->my_seq = 1;
    s->dirty = false;
    s->db.assign(num_nodes, LSARecord());
    s->next_hop.assign(num_nodes, -1);

    LSARecord& me = s->db[my_id];
    me.valid = true;
    me.seq = 1;
    for (int i = 0; i < num_neighbors; ++i) {
#if USE_COST
        me.nbrs.push_back(std::make_pair(neighbor_ids[i], link_costs[i]));
#else
        (void)link_costs;
        me.nbrs.push_back(std::make_pair(neighbor_ids[i], 1));
#endif
    }
    std::sort(me.nbrs.begin(), me.nbrs.end());

    recompute(s);

    if (num_neighbors > 0) {
        std::vector<int> origins;
        origins.push_back(my_id);
        std::vector<uint8_t> buf;
        encode_records(s, origins, buf);
        flood(s, buf, -1, -1);
    }
    return s;
}

extern "C" void on_link_change(RouterState* s, int neighbor, int new_cost) {
    LSARecord& me = s->db[s->my_id];
    int old_cost = -1;
    for (size_t i = 0; i < me.nbrs.size(); ++i) {
        if (me.nbrs[i].first == neighbor) { old_cost = me.nbrs[i].second; break; }
    }
    bool was = (old_cost >= 0);
    bool was_reachable = was || (neighbor >= 0 && neighbor < s->num_nodes
                                 && s->next_hop[neighbor] != -1);

    bool changed = false;
    if (new_cost == NETSIM2_NO_LINK) {
        if (was) {
            for (size_t i = 0; i < me.nbrs.size(); ++i) {
                if (me.nbrs[i].first == neighbor) {
                    me.nbrs.erase(me.nbrs.begin() + i);
                    break;
                }
            }
            changed = true;
        }
        s->to_sync.erase(neighbor);
    } else if (!was) {
        me.nbrs.push_back(std::make_pair(neighbor, new_cost));
        std::sort(me.nbrs.begin(), me.nbrs.end());
        changed = true;
        if (!was_reachable) s->to_sync.insert(neighbor);
#if USE_COST
    } else if (old_cost != new_cost) {
        for (size_t i = 0; i < me.nbrs.size(); ++i) {
            if (me.nbrs[i].first == neighbor) { me.nbrs[i].second = new_cost; break; }
        }
        changed = true;
#endif
    }

    if (changed) {
        recompute(s);
        s->dirty = true;
        schedule_wakeup(get_now());
    } else if (!s->to_sync.empty()) {
        schedule_wakeup(get_now());
    }
}

extern "C" void on_control(RouterState* s, int from,
                           const uint8_t* payload, int len) {
    if (len < 1) return;
    int idx = 0;
    std::vector<int> updated;
    while (idx < len) {
        if (idx + 3 > len) break;
        int origin = payload[idx++];
        uint8_t seq = payload[idx++];
        int num = payload[idx++];
        std::vector<std::pair<int,int> > nbrs;
        bool trunc = false;
        for (int j = 0; j < num; ++j) {
#if USE_COST
            if (idx + 2 > len) { trunc = true; break; }
            int id = payload[idx++];
            int c = payload[idx++];
            nbrs.push_back(std::make_pair(id, c));
#else
            if (idx + 1 > len) { trunc = true; break; }
            int id = payload[idx++];
            nbrs.push_back(std::make_pair(id, 1));
#endif
        }
        if (trunc) break;
        if (origin < 0 || origin >= s->num_nodes) continue;
        if (origin == s->my_id) continue;
        LSARecord& r = s->db[origin];
        if (!r.valid || seq_greater(seq, r.seq)) {
            r.valid = true;
            r.seq = seq;
            r.nbrs = nbrs;
            updated.push_back(origin);
        }
    }
    if (!updated.empty()) {
        recompute(s);
        std::vector<uint8_t> buf;
        encode_records(s, updated, buf);
        int ex2 = (updated.size() == 1) ? updated[0] : -1;
        flood(s, buf, from, ex2);
    }
}

extern "C" int on_packet(RouterState* s, int dst) {
    if (dst < 0 || dst >= s->num_nodes) return -1;
    int nh = s->next_hop[dst];
    if (nh != -1) return nh;

    std::vector<int> relaxed;
    compute(s, relaxed, true);
    return relaxed[dst];
}

extern "C" void on_timer(RouterState* s) {
    if (!s->dirty && s->to_sync.empty()) return;

    if (s->dirty) {
        s->my_seq++;
        s->db[s->my_id].seq = s->my_seq;
        s->dirty = false;
    }

    std::vector<uint8_t> mine;
    {
        std::vector<int> o;
        o.push_back(s->my_id);
        encode_records(s, o, mine);
    }

    bool need_sync = !s->to_sync.empty();
    const std::vector<std::pair<int,int> >& me = s->db[s->my_id].nbrs;
    for (size_t i = 0; i < me.size(); ++i) {
        int nb = me[i].first;
        if (need_sync && s->to_sync.count(nb)) {
            send_full_db(s, nb);
        } else {
            send_control(nb, mine.data(), (int)mine.size());
        }
    }
    s->to_sync.clear();
}

extern "C" void router_shutdown(RouterState* s) {
    delete s;
}
