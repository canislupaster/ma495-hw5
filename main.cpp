#include <algorithm>
#include <cairomm/fontface.h>
#include <cairomm/surface.h>
#include <cairomm/types.h>
#include <chrono>
#include <iostream>
#include <random>
#include <fstream>
#include <set>
#include <thread>
#include <unordered_set>
#include <vector>

#include <cairomm/cairomm.h>

#include <GeographicLib/Geodesic.hpp>
#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;
using namespace GeographicLib; 
using namespace Cairo;

//https://stackoverflow.com/a/64090995
array<double,3> hsl2rgb(double h,double s,double l) {
   double a=s*min(l,1-l);
   auto f = [=](double n) {
		double k=fmod(n+h/30, 12);
		return l - a*max(min({k-3,9-k,1.0}),-1.0);
	 };
   return {f(0),f(8),f(4)};
}

int main() {
	json parks = json::parse(ifstream("./parks.json"))["elements"];
	int n = parks.size();

	vector<array<double,2>> lat_long;
	for (int i=0; i<n; i++) {
		lat_long.push_back({
			parks[i]["center"]["lat"],
			parks[i]["center"]["lon"]
		});
	}

	struct E {
		double dist;
		mutable int i,j;
		bool operator<(E const& other) const {
			return dist>other.dist;
		}
	};

	vector<priority_queue<E>> dist(n);
	Geodesic const& geo = Geodesic::WGS84();

	priority_queue<E> all;

	cout<<"computing distances...\n";

	for (int i=0; i<n-1; i++) {
		for (int j=i+1; j<n; j++) {
			double distance;
			geo.Inverse(lat_long[i][0], lat_long[i][1], lat_long[j][0], lat_long[j][1], distance);
			dist[i].push(E {
				.dist=distance,
				.i=i, .j=j
			});
		}

		all.push(dist[i].top());
		dist[i].pop();
		cout<<i<<"/"<<n<<"\n";
	}

	vector<int> root(n), rank(n,1);
	constexpr double inf = numeric_limits<double>::infinity();
	vector<double> death(n, inf);
	iota(root.begin(), root.end(), 0);

	cout<<"computing components...\n";

	int k=1;
	double max_death=0;
	while (all.size()) {
		auto e = all.top(); all.pop();

		int orig_i=e.i;
		while (root[e.i]!=e.i) e.i=root[e.i];
		while (root[e.j]!=e.j) e.j=root[e.j];

		if (e.i!=e.j) {
			if (rank[e.i]<rank[e.j]) swap(e.i, e.j);

			root[e.j] = e.i;
			rank[e.i] += rank[e.j];
			death[e.j] = e.dist;

			max_death=max(max_death,death[e.j]);

			cout<<k<<"/"<<n<<"\n";
			if (++k == n) {
				break;
			}
		}

		while (dist[orig_i].size()) {
			auto& j = dist[orig_i].top().j;
			while (j!=root[j]) j=root[j];

			if (j!=e.i) {
				all.push(dist[orig_i].top());
				dist[orig_i].pop();
				break;
			}

			dist[orig_i].pop();
		}
	}

	cout<<"drawing\n";
	sort(death.rbegin(), death.rend());

	constexpr int h=3000, w=1500, nxtics=7, nytics=nxtics*3;
	constexpr double b_margin=50, l_margin=100, draw_h=h-b_margin, draw_w=w-l_margin;
	RefPtr<SvgSurface> surface = SvgSurface::create("plot.svg",w,h);
	RefPtr<Context> cr = Context::create(surface);

	array<double,2> prev = {w,0};
	for (int i=0; i<n; i++) {
		double prop = min(death[i]/max_death,1.0);

		cr->move_to(l_margin, prev[1]);
		cr->line_to(prev[0], prev[1]);
		cr->line_to(prev[0]=l_margin + draw_w*prop, prev[1]=draw_h*(i+1)/n);
		cr->line_to(l_margin, prev[1]);

		auto [r,g,b] = hsl2rgb(240*(1.0-log(prop)/log(3000)), 1.0, 0.5);
		cr->set_source_rgb(r,g,b);
		cr->fill();
	}

	constexpr double rise = 20.0;

	cr->set_source_rgb(0.7, 0.7, 0.7);
	cr->set_dash(vector<double>{10.0,10.0}, 5.0);
	cr->set_line_width(3.0);

	for (int i=0; i<=nxtics; i++) {
		cr->move_to(l_margin + draw_w*double(i)/nxtics, 0);
		cr->line_to(l_margin + draw_w*double(i)/nxtics, draw_h);
		cr->stroke();
	}

	for (int i=0; i<=nytics; i++) {
		cr->move_to(l_margin, draw_h*double(i)/nytics);
		cr->line_to(w, draw_h*double(i)/nytics);
		cr->stroke();
	}

	cr->unset_dash();
	cr->set_source_rgb(0,0,0);

	cr->move_to(l_margin, 0);
	cr->line_to(l_margin, draw_h);
	cr->stroke();

	cr->move_to(l_margin, draw_h);
	cr->line_to(w, draw_h);
	cr->stroke();

	cr->select_font_face("Times New Roman", ToyFontFace::Slant::NORMAL, ToyFontFace::Weight::BOLD);
	cr->set_font_size(24.0);

	FontExtents fext;
	cr->get_font_extents(fext);

	auto label = [&cr,desc=fext.descent](double x, double y, double m, bool vert) {
		string s = isinf(m) ? "∞" : format("{} meters", round(m));
		TextExtents ext;
		cr->get_text_extents(s, ext);
		cr->move_to(x-ext.width/2, y+ext.height/2-(vert ? 0 : desc));
		cr->text_path(s);
		cr->fill();
	};

	for (int i=0; i<=nxtics; i++) {
		cr->move_to(l_margin + draw_w*double(i)/nxtics, draw_h - rise);
		cr->line_to(l_margin + draw_w*double(i)/nxtics, draw_h);
		cr->stroke();

		label(l_margin + draw_w*double(i)/nxtics, draw_h + b_margin/2, double(i)/nxtics * max_death, false);
	}

	for (int i=0; i<=nytics; i++) {
		cr->move_to(l_margin, draw_h*double(i)/nytics);
		cr->line_to(l_margin+rise, draw_h*double(i)/nytics);
		cr->stroke();

		int yi = floor(double(i)/(nytics+1) * n);
		label(l_margin/2, draw_h*double(i)/nytics, death[yi], true);
	}
}
 