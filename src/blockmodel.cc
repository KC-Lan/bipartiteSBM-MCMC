#include <iostream>
#include "blockmodel.hh"
#include "graph_utilities.hh"  // for the is_disjoint function
#include "output_functions.hh"

#include "support/cache.hh"

using namespace std;

/** Default constructor */
blockmodel_t::blockmodel_t(const uint_vec_t &memberships, uint_vec_t types, size_t g, size_t KA,
                           size_t KB, double epsilon, const adj_list_t *adj_list_ptr, bool is_bipartite) :
        random_block_(0, g - 1), random_node_(0, (*adj_list_ptr).size() - 1), adj_list_ptr_(adj_list_ptr),
        types_(std::move(types)) {
    KA_ = KA;
    KB_ = KB;
    epsilon_ = epsilon;
    memberships_ = memberships;
    n_.resize(g, 0);
    deg_.resize(memberships.size(), 0);
    num_edges_ = 0;
    entropy_from_degree_correction_ = 0.;

    for (size_t j = 0; j < memberships.size(); ++j) {
        if (types_[j] == 0) {
            nsize_A_ += 1;
        } else if (types_[j] == 1) {
            nsize_B_ += 1;
        }

        ++n_[memberships[j]];

        for (auto nb = adj_list_ptr_->at(j).begin(); nb != adj_list_ptr_->at(j).end(); ++nb) {
            ++deg_[j];
            ++num_edges_;
        }
    }
    num_edges_ /= 2;
    init_cache(num_edges_);

    double deg_factorial = 0;
    for (size_t node = 0; node < memberships.size(); ++node) {
        deg_factorial = 0;

        for (size_t deg = 1; deg <= deg_[node]; ++deg) {
            deg_factorial += safelog_fast(deg);
        }
        entropy_from_degree_correction_ += deg_factorial;
    }
    compute_k();
    compute_m();
    compute_m_r();

    // Note that Tiago's MCMC proposal jumps has to randomly access elements in an adjacency list
    // Here, we define an vectorized data structure to make such data access O(1) [else it'll be O(n)].
    adj_list_.resize(adj_list_ptr_->size());
    for (size_t i = 0; i < adj_list_ptr_->size(); ++i) {
        adj_list_[i].resize(adj_list_ptr_->at(i).size(), 0);
    }
    for (size_t node = 0; node < memberships_.size(); ++node) {
        size_t idx = 0;
        for (auto nb = adj_list_ptr_->at(node).begin(); nb != adj_list_ptr_->at(node).end(); ++nb) {
            adj_list_[node][idx] = size_t(*nb);
            ++idx;
        }
    }
}

const int_vec_t *blockmodel_t::get_k(size_t vertex) const noexcept { return &k_[vertex]; }

const int blockmodel_t::get_degree(size_t vertex) const noexcept { return deg_.at(vertex); }

const uint_vec_t *blockmodel_t::get_memberships() const noexcept { return &memberships_; }

double blockmodel_t::get_epsilon() const noexcept { return epsilon_; }

const uint_mat_t *blockmodel_t::get_m() const noexcept { return &m_; }

const uint_vec_t *blockmodel_t::get_m_r() const noexcept { return &m_r_; }

size_t blockmodel_t::get_N() const noexcept { return memberships_.size(); }

size_t blockmodel_t::get_g() const noexcept { return n_.size(); }

size_t blockmodel_t::get_KA() const noexcept { return KA_; }

bool blockmodel_t::apply_mcmc_moves(std::vector<mcmc_state_t> &&moves) noexcept {
    for (auto const &mv: moves) {
        __source__ = mv.source;
        __target__ = mv.target;
        __vertex__ = mv.vertex;

        --n_[__source__];
        if (n_[__source__] == 0) {  // No move that makes an empty group will be allowed
            ++n_[__source__];
            return false;
        }
        ++n_[__target__];

        ki_ = get_k(__vertex__);
        size_t ki_size = ki_->size();
        for (size_t i = 0; i < ki_size; ++i) {
            int ki_at_i = ki_->at(i);
            if (ki_at_i != 0) {
                m_[__source__][i] -= ki_at_i;
                m_[__target__][i] += ki_at_i;
                m_[i][__source__] = m_[__source__][i];
                m_[i][__target__] = m_[__target__][i];
            }
        }
        m_r_[__source__] -= deg_[__vertex__];
        m_r_[__target__] += deg_[__vertex__];
        // Change block degrees and block sizes
        for (auto const &neighbour: adj_list_ptr_->at(__vertex__)) {
            --k_[neighbour][__source__];
            ++k_[neighbour][__target__];
        }
        // Set new memberships
        memberships_[__vertex__] = unsigned(int(__target__));
    }
    return true;
}

void blockmodel_t::shuffle_bisbm(std::mt19937 &engine, size_t NA, size_t NB) noexcept {
    std::shuffle(&memberships_[0], &memberships_[NA], engine);
    std::shuffle(&memberships_[NA], &memberships_[NA + NB], engine);
    compute_k();
    compute_m();
    compute_m_r();
}

inline void blockmodel_t::compute_k() noexcept {
    // In principle, this function only executes once.
    k_.clear();
    k_.resize(adj_list_ptr_->size());
    for (size_t i = 0; i < adj_list_ptr_->size(); ++i) {
        k_[i].resize(this->n_.size(), 0);
        for (auto nb = adj_list_ptr_->at(i).begin(); nb != adj_list_ptr_->at(i).end(); ++nb) {
            ++k_[i][memberships_[*nb]];
        }
    }
}

inline void blockmodel_t::compute_m() noexcept {
    // In principle, this function only executes once.
    m_.clear();
    m_.resize(get_g());
    for (auto i = 0; i < get_g(); ++i) {
        m_[i].resize(get_g(), 0);
    }
    for (size_t vertex = 0; vertex < adj_list_ptr_->size(); ++vertex) {
        __vertex__ = memberships_[vertex];
        for (auto const &nb: adj_list_ptr_->at(vertex)) {
            ++m_[__vertex__][memberships_[nb]];
        }
    }

}

inline void blockmodel_t::compute_m_r() noexcept {
    // In principle, this function only executes once.
    m_r_.clear();
    m_r_.resize(get_g(), 0);
    size_t _m_r = 0;
    for (size_t r = 0; r < get_g(); ++r) {
        _m_r = 0;
        for (size_t s = 0; s < get_g(); ++s) {
            _m_r += m_[r][s];
        }
        m_r_[r] = unsigned(int(_m_r));
    }
}
