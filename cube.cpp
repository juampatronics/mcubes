#include "geom.h"
#include <cassert>
#include <iomanip>
#include <cstdio>
#include <bitset>

struct tet {
    int a, b, c, d;
};

int cubefaces[6][4] = {
    {0, 2, 6, 4}, {1, 5, 7, 3},
    {0, 4, 5, 1}, {2, 3, 7, 6},
    {0, 1, 3, 2}, {4, 6, 7, 5}
};

int cubeedge(int v1, int v2) {
    // v1 = x1 + 2y1 + 4z1
    // v2 = x2 + 2y2 + 4z2
    int x1, x2, y1, y2, z1, z2;
    x1 = v1 & 1;
    x2 = v2 & 1;
    y1 = (v1 & 2) >> 1;
    y2 = (v2 & 2) >> 1;
    z1 = v1 >> 2;
    z2 = v2 >> 2;
    if (x1 != x2) {
        assert(y1 == y2 && z1 == z2);
        return y1 + 2 * z1;
    }
    if (y1 != y2) {
        assert(x1 == x2 && z1 == z2);
        return 4 + x1 + 2 * z1;
    }
    assert(x1 == x2 && y1 == y2);
    return 8 + x1 + 2 * y1;
}

#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

#include <Eigen/Core>

void save(const std::string &filename, const std::vector<point> &pts, const std::vector<tet> &tets, const std::vector<double> &vals) {
    std::ofstream f(filename, std::ios::binary);
    f << "# vtk DataFile Version 3.0\n";
    f << "Cube\n";
    f << "ASCII\n";
    f << "DATASET UNSTRUCTURED_GRID\n";
    f << "POINTS " << pts.size() << " float\n";
    for (const auto &p : pts)
        f << p.x << " " << p.y << " " << p.z << "\n";
    f << "CELLS " << tets.size() << " " << 5 * tets.size() << "\n";
    for (const auto &t : tets)
        f << "4 " << t.a << " " << t.b << " " << t.c << " " << t.d << "\n";
    f << "CELL_TYPES " << tets.size() << "\n";
    std::for_each(tets.begin(), tets.end(), [&f] (const tet &) { f << "10\n"; });
    f << "POINT_DATA " << pts.size() << "\n";
    f << "SCALARS u float\nLOOKUP_TABLE default\n";
    for (const auto &v: vals)
        f << v << "\n";
}

int edgeid(int u, int v) {
    return std::min(u, v) * 100 + std::max(u, v);
}

int main() {
    std::vector<point> pts;

    for (int z = 0; z < 2; z++)
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 2; x++)
                pts.emplace_back(point(x, y, z));

    for (int f = 0; f < 6; f++) {
        point sum(0, 0, 0);
        for (int v = 0; v < 4; v++)
            sum += pts[cubefaces[f][v]];
        pts.push_back(sum * 0.25);
    }

    pts.emplace_back(point(.5, .5, .5));

    std::vector<tet> tets;

    for (int f = 0; f < 6; f++)
        for (int j = 0; j < 4; j++)
            tets.emplace_back(tet{cubefaces[f][(j+1)&3], cubefaces[f][j], 8+f, 14});

    std::unordered_map<int, int> edgemap;
    std::vector<int> iedgemap;
    for (const auto &t : tets) {
        int p[4] = {t.a, t.b, t.c, t.d};
        int i[6] = {0, 0, 0, 1, 1, 2};
        int j[6] = {1, 2, 3, 2, 3, 3};
        for (int k = 0; k < 6; k++) {
            int id = edgemap.size();
            int eid = edgeid(p[i[k]], p[j[k]]);
            if (edgemap.find(eid) == edgemap.end()) {
                edgemap[eid] = id;
                iedgemap.push_back(eid);
            }
        }
    }

    std::vector<std::unordered_set<int>> graph(edgemap.size());

    for (const auto &t : tets) {
        int p[4] = {t.a, t.b, t.c, t.d};
        int i[6] = {0, 0, 0, 1, 1, 2};
        int j[6] = {1, 2, 3, 2, 3, 3};
        for (int k = 0; k < 6; k++) {
            int eid = edgemap[edgeid(p[i[k]], p[j[k]])];
            for (int ks = k + 1; ks < 6; ks++) {
                int eids = edgemap[edgeid(p[i[ks]], p[j[ks]])];
                graph[eid].insert(eids);
                graph[eids].insert(eid);
            }
        }
    }

    std::vector<double> vals(pts.size());

    double theta = .7;

    std::vector<int> marks(edgemap.size());

    FILE *lut = fopen("lut.h", "wb");
    fprintf(lut, "static unsigned int edgeGroup[256] = {");

    for (int cases = 0; cases < (1 << 8); cases++) {
        std::cout << "*** CASE " << cases << " ***" << std::endl;

        for (int k = 0; k < 8; k++)
            vals[k] = (cases & (1 << k)) ? 1 : 0;

        vals[14] = 0;
        for (int k = 0; k < 8; k++)
            vals[14] += vals[k];
        vals[14] *= 0.125;

        for (int f = 0; f < 6; f++) {
            vals[8 + f] = 0;
            for (int j = 0; j < 4; j++)
                vals[8 + f] += vals[cubefaces[f][j]];
            vals[8 + f] *= 0.25;
        }

        for (size_t e = 0; e < edgemap.size(); e++) {
            int eid = iedgemap[e];
            int u = eid % 100;
            int v = eid / 100;
            marks[e] = (vals[u] > theta) ^ (vals[v] > theta) ? 0 : -1;
        }

        assert(graph.size() == 50);

        Eigen::Matrix<int, 50, 50> A, B;
        A.setZero();

        for (size_t u = 0; u < graph.size(); u++) {
            if (marks[u] == -1)
                continue;
            A(u, u) = 1;
            for (int v : graph[u]) {
                if (marks[v] == -1)
                    continue;
                A(u, v) = A(v, u) = 1;
            }
        }
        A.swap(B);
        do {
            A.swap(B);
            B.noalias() = (A * A).unaryExpr([] (int v) { return v > 0; } ).cast<int>();
        } while (B != A);

        int color = 1;
        for (size_t u = 0; u < graph.size(); u++) {
            if (marks[u] == -1)
                continue;
            if (marks[u] != 0)
                continue;
            for (size_t v = 0; v < graph.size(); v++)
                if (A(u, v) == 1) {
                    assert(marks[v] == 0);
                    marks[v] = color;
                }
            color++;
        }

        std::cout << "disjoint patches: " << color - 1 << std::endl;

        int code = 0;

        for (const auto &e : edgemap) {
            int v1, v2;
            v1 = e.first / 100;
            v2 = e.first % 100;
            if (v1 < 8 && v2 < 8) {
                int ceid = cubeedge(v1, v2);
                int mkid = marks[e.second];
                assert(mkid != 0);
                if (mkid < 0)
                    mkid = 3;
                else
                    mkid--;
                assert(mkid < 4 && mkid >= 0);
                code |= mkid << (2 * ceid);
                std::cout << "(" << v1 << ", " << v2 << ") [" << ceid << "] -> " << marks[e.second] << std::endl;
            }
        }
        if ((cases % 8) == 0)
            fprintf(lut, "\n\t");
        fprintf(lut, "0x%06x,", code);
        std::cout << std::bitset<24>(code) << std::endl;

        std::ofstream dot("cube." + std::to_string(cases) + ".dot", std::ios::binary);

        dot << "strict graph cube {\n";
        for (size_t u = 0; u < graph.size(); u++) {
            if (marks[u] == -1)
                continue;
            int eu = iedgemap[u];
            if ((eu / 100) < 8 && (eu % 100) < 8) {
                dot << "e" << eu / 100 << "_" << eu % 100;
                dot << " [style = bold];\n";
            }
            dot << "e" << eu / 100 << "_" << eu % 100 << " -- {";
            for (int v : graph[u]) {
                int ev = iedgemap[v];
                if (marks[v] == -1)
                    continue;
                dot << "e" << ev / 100 << "_" << ev % 100 << " ";
            }
            dot << "}\n";
        }
        dot << "}\n";

        save("cube." + std::to_string(cases) + ".vtk", pts, tets, vals);
    }
    fprintf(lut, "\n};\n");
    fclose(lut);

    return 0;
}
