#pragma once

#include "standard.hpp"
namespace debruijn_graph {

//This class corrects mismatches or masks repeat differences or other such things with the sequence of an edge
template<class Graph>
class ContigCorrector {
private:
	typedef typename Graph::EdgeId EdgeId;
	const Graph &graph_;
protected:
	const Graph &graph() const {
		return graph_;
	}

public:
	ContigCorrector(const Graph &graph) : graph_(graph) {
	}

	virtual string correct(EdgeId e) = 0;

	virtual ~ContigCorrector() {
	}
};

template<class Graph>
class DefaultContigCorrector : public ContigCorrector<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;
public:
	DefaultContigCorrector(const Graph &graph) : ContigCorrector<Graph>(graph) {
	}

	string correct(EdgeId e) {
		return this->graph().EdgeNucls(e).str();
	}
};

template<class Graph>
class MaskingContigCorrector : public ContigCorrector<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;
	MismatchMasker<Graph>& mismatch_masker_;
public:
	MaskingContigCorrector(const Graph &graph, MismatchMasker<Graph>& mismatch_masker) : ContigCorrector<Graph>(graph), mismatch_masker_(mismatch_masker) {
	}

	string correct(EdgeId e) {
	    return mismatch_masker_.MaskedEdgeNucls(e, 0.00001);
	}
};

//This class uses corrected sequences to construct contig (just return as is, find unipath, trim contig)
template<class Graph>
class ContigConstructor {
private:
	typedef typename Graph::EdgeId EdgeId;
	const Graph &graph_;
	ContigCorrector<Graph> &corrector_;
protected:
	string correct(EdgeId e) {
		return corrector_.correct(e);
	}

	const Graph &graph() const {
		return graph_;
	}

public:

	ContigConstructor(const Graph &graph, ContigCorrector<Graph> &corrector) : graph_(graph), corrector_(corrector) {
	}

	virtual pair<string, double> construct(EdgeId e) = 0;

	virtual ~ContigConstructor(){
	}
};

template<class Graph>
class DefaultContigConstructor : public ContigConstructor<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;
public:

	DefaultContigConstructor(const Graph &graph, ContigCorrector<Graph> &corrector) : ContigConstructor<Graph>(graph, corrector) {
	}

	pair<string, double> construct(EdgeId e) {
		return make_pair(this->correct(e), this->graph().coverage(e));
	}
};

template<class Graph>
class UnipathConstructor : public ContigConstructor<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;

	string MergeOverlappingSequences(std::vector<string>& ss, size_t overlap) {
		if (ss.empty()) {
			return "";
		}
		stringstream result;
		result << ss.front().substr(0, overlap);
//		prev_end = ss.front().substr(0, overlap);
		for (auto it = ss.begin(); it != ss.end(); ++it) {
//			VERIFY(prev_end == it->substr(0, overlap));
			result << it->substr(overlap);
//			prev_end = it->substr(it->size() - overlap);
		}
		return result.str();
	}


	string MergeSequences(const Graph& g,
			const vector<typename Graph::EdgeId>& continuous_path) {
		vector<string> path_sequences;
		for (size_t i = 0; i < continuous_path.size(); ++i) {
			if(i > 0)
				VERIFY(
					g.EdgeEnd(continuous_path[i - 1])
							== g.EdgeStart(continuous_path[i]));
			path_sequences.push_back(this->correct(continuous_path[i]));
		}
		return MergeOverlappingSequences(path_sequences, g.k());
	}

public:

	UnipathConstructor(const Graph &graph, ContigCorrector<Graph> &corrector) : ContigConstructor<Graph>(graph, corrector) {
	}

	pair<string, double> construct(EdgeId e) {
		vector<EdgeId> unipath = Unipath(this->graph(), e);
		return make_pair(MergeSequences(this->graph(), unipath), AvgCoverage(this->graph(), unipath));
	}
};

template<class Graph>
class CuttingContigConstructor : public ContigConstructor<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;

	bool ShouldCut(VertexId v) const {
		const Graph &g = this->graph();
		vector<EdgeId> edges = g.OutgoingEdges(v);
		if(edges.size() == 0)
			return false;
		for(size_t i = 1; i < edges.size(); i++) {
			if(g.EdgeNucls(edges[i])[g.k()] != g.EdgeNucls(edges[0])[g.k()])
				return false;
		}
		edges = g.IncomingEdges(v);
		for(size_t i = 0; i < edges.size(); i++)
			for(size_t j = i + 1; j < edges.size(); j++) {
				if(g.EdgeNucls(edges[i])[g.length(edges[i]) - 1] != g.EdgeNucls(edges[j])[g.length(edges[j]) - 1])
					return true;
			}
		return false;
	}

public:

	CuttingContigConstructor(const Graph &graph, ContigCorrector<Graph> &corrector) : ContigConstructor<Graph>(graph, corrector) {
	}

	pair<string, double> construct(EdgeId e) {
		string result = this->correct(e);
		if(result.size() > this->graph().k() && ShouldCut(this->graph().EdgeEnd(e))) {
			result = result.substr(0, result.size() - this->graph().k());
		}
		if(result.size() > this->graph().k() && ShouldCut(this->graph().conjugate(this->graph().EdgeStart(e)))) {
			result = result.substr(this->graph().k(), result.size());
		}
		return make_pair(result, this->graph().coverage(e));
	}
};

template<class Graph>
class ContigPrinter {
private:
	const Graph &graph_;
	ContigConstructor<Graph> &constructor_;

	template<class sequence_stream>
	void ReportEdge(osequencestream_cov& oss
			, const pair<string, double> sequence_data) {
		oss << sequence_data.second;
		oss << sequence_data.first;
	}

public:
	ContigPrinter(const Graph &graph, ContigConstructor<Graph> &constructor) : graph_(graph), constructor_(constructor) {
	}

	template<class sequence_stream>
	void PrintContigs(sequence_stream &os) {
		for (auto it = graph_.SmartEdgeBegin(); !it.IsEnd(); ++it) {
			ReportEdge<sequence_stream>(os, constructor_.construct(*it));
			it.HandleDelete(graph_.conjugate(*it));
		}
	}
};

template<class Graph>
void ReportEdge(osequencestream_cov& oss
		, const Graph& g
		, typename Graph::EdgeId e
		, bool output_unipath = false
		, size_t solid_edge_length_bound = 0) {
	typedef typename Graph::EdgeId EdgeId;
	if (!output_unipath || (PossibleECSimpleCheck(g, e) && g.length(e) <= solid_edge_length_bound)) {
		TRACE("Outputting edge " << g.str(e) << " as single edge");
		oss << g.coverage(e);
		oss << g.EdgeNucls(e);
	} else {
		TRACE("Outputting edge " << g.str(e) << " as part of unipath");
		vector<EdgeId> unipath = Unipath(g, e);
		TRACE("Unipath is " << g.str(unipath));
		oss << AvgCoverage(g, unipath);
		TRACE("Merged sequence is of length " << MergeSequences(g, unipath).size());
		oss << MergeSequences(g, unipath);
	}
}

template<class Graph>
void ReportMaskedEdge(osequencestream_cov& oss
		, const Graph& g
		, typename Graph::EdgeId e
    , MismatchMasker<Graph>& mismatch_masker
		, bool output_unipath = false
		, size_t solid_edge_length_bound = 0) {
	typedef typename Graph::EdgeId EdgeId;
	if (!output_unipath || (PossibleECSimpleCheck(g, e) && g.length(e) <= solid_edge_length_bound)) {
		TRACE("Outputting edge " << g.str(e) << " as single edge");
		oss << g.coverage(e);
    const string& s = mismatch_masker.MaskedEdgeNucls(e, 0.00001);
		oss << s;
	} else {
		//support unipath
		TRACE("Outputting edge " << g.str(e) << " as part of unipath");
    const vector<EdgeId>& unipath = Unipath(g, e);
		TRACE("Unipath is " << g.str(unipath));
		oss << AvgCoverage(g, unipath);
		TRACE("Merged sequence is of length " << MergeSequences(g, unipath).size());
		oss << MergeSequences(g, unipath);
	}
}

void OutputContigs(NonconjugateDeBruijnGraph& g,
		const string& contigs_output_filename,
		bool output_unipath = false,
		size_t solid_edge_length_bound = 0) {
	INFO("Outputting contigs to " << contigs_output_filename);
	osequencestream_cov oss(contigs_output_filename);
	for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
		ReportEdge(oss, g, *it, output_unipath, solid_edge_length_bound);
	}
	DEBUG("Contigs written");
}

void OutputContigs(ConjugateDeBruijnGraph& g,
		const string& contigs_output_filename,
		bool output_unipath = false,
		size_t solid_edge_length_bound = 0) {
	INFO("Outputting contigs to " << contigs_output_filename);
	DefaultContigCorrector<ConjugateDeBruijnGraph> corrector(g);
	osequencestream_cov oss(contigs_output_filename);
	if(!output_unipath) {
		DefaultContigConstructor<ConjugateDeBruijnGraph> constructor(g, corrector);
		ContigPrinter<ConjugateDeBruijnGraph>(g, constructor).PrintContigs(oss);
	} else {
		UnipathConstructor<ConjugateDeBruijnGraph> constructor(g, corrector);
		ContigPrinter<ConjugateDeBruijnGraph>(g, constructor).PrintContigs(oss);
	}
//	{
//		osequencestream_cov oss(contigs_output_filename);
//		set<ConjugateDeBruijnGraph::EdgeId> edges;
//		for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
//			if (edges.count(*it) == 0) {
//				ReportEdge(oss, g, *it, output_unipath, solid_edge_length_bound + ".oppa.fasta");
//				edges.insert(g.conjugate(*it));
//			}
//			//		oss << g.EdgeNucls(*it);
//		}
//		DEBUG("Contigs written");
//	}
//	if(!output_unipath) {
//		OutputContigs(g, contigs_output_filename + ".2.fasta", true, solid_edge_length_bound);
//	}
}

bool ShouldCut(ConjugateDeBruijnGraph& g, VertexId v) {
	vector<EdgeId> edges = g.OutgoingEdges(v);
	if(edges.size() == 0)
		return false;
	for(size_t i = 1; i < edges.size(); i++) {
		if(g.EdgeNucls(edges[i])[g.k()] != g.EdgeNucls(edges[0])[g.k()])
			return false;
	}
	edges = g.IncomingEdges(v);
	for(size_t i = 0; i < edges.size(); i++)
		for(size_t j = i + 1; j < edges.size(); j++) {
			if(g.EdgeNucls(edges[i])[g.length(edges[i]) - 1] != g.EdgeNucls(edges[j])[g.length(edges[j]) - 1])
				return true;
		}
	return false;
}

void OutputCutContigs(ConjugateDeBruijnGraph& g,
		const string& contigs_output_filename,
		bool output_unipath = false,
		size_t solid_edge_length_bound = 0) {
	INFO("Outputting contigs to " << contigs_output_filename);
	DefaultContigCorrector<ConjugateDeBruijnGraph> corrector(g);
	osequencestream_cov oss(contigs_output_filename);
	CuttingContigConstructor<ConjugateDeBruijnGraph> constructor(g, corrector);
	ContigPrinter<ConjugateDeBruijnGraph>(g, constructor).PrintContigs(oss);

//	osequencestream_cov oss(contigs_output_filename);
//	set<ConjugateDeBruijnGraph::EdgeId> edges;
//	for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
//		EdgeId e = *it;
//		cout << g.length(e) << endl;
//		if (edges.count(e) == 0) {
//			Sequence s = g.EdgeNucls(e);
//			cout << s.size() << endl;
//			cout << "oppa " << ShouldCut(g, g.EdgeEnd(e)) << endl;
//			if(s.size() > g.k() && ShouldCut(g, g.EdgeEnd(e))) {
//				s = s.Subseq(0, s.size() - g.k());
//				cout << s.size() << endl;
//			}
//			cout << "oppa1 " << ShouldCut(g, g.conjugate(g.EdgeStart(e))) << endl;
//			if(s.size() > g.k() && ShouldCut(g, g.conjugate(g.EdgeStart(e)))) {
//				s = s.Subseq(g.k(), s.size());
//				cout << s.size() << endl;
//			}
//			oss << g.coverage(e);
//			oss << s;
//			edges.insert(g.conjugate(*it));
//		}
//		//		oss << g.EdgeNucls(*it);
//	}
}

void OutputMaskedContigs(ConjugateDeBruijnGraph& g,
		const string& contigs_output_filename, MismatchMasker<ConjugateDeBruijnGraph>& masker,
		bool output_unipath = false,
		size_t solid_edge_length_bound = 0) {
	INFO("Outputting contigs with masked mismatches to " << contigs_output_filename);
	MaskingContigCorrector<ConjugateDeBruijnGraph> corrector(g, masker);
	osequencestream_cov oss(contigs_output_filename);
	if(!output_unipath) {
		DefaultContigConstructor<ConjugateDeBruijnGraph> constructor(g, corrector);
		ContigPrinter<ConjugateDeBruijnGraph>(g, constructor).PrintContigs(oss);
	} else {
		UnipathConstructor<ConjugateDeBruijnGraph> constructor(g, corrector);
		ContigPrinter<ConjugateDeBruijnGraph>(g, constructor).PrintContigs(oss);
	}
//	osequencestream_cov oss(contigs_output_filename);
//	set<ConjugateDeBruijnGraph::EdgeId> edges;
//	for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
//		if (edges.count(*it) == 0) {
//			ReportMaskedEdge(oss, g, *it, masker, output_unipath, solid_edge_length_bound);
//			edges.insert(g.conjugate(*it));
//		}
//		//		oss << g.EdgeNucls(*it);
//	}
	DEBUG("Contigs written");
}

void OutputSingleFileContigs(NonconjugateDeBruijnGraph& g,
		const string& contigs_output_dir) {
	INFO("Outputting contigs to " << contigs_output_dir);
	int n = 0;
	make_dir(contigs_output_dir);
	char n_str[20];
	for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
		sprintf(n_str, "%d.fa", n);

		osequencestream oss(contigs_output_dir + n_str);

		//		osequencestream oss(contigs_output_dir + "tst.fasta");
		oss << g.EdgeNucls(*it);
		n++;
	}DEBUG("SingleFileContigs written");
}

void OutputSingleFileContigs(ConjugateDeBruijnGraph& g,
		const string& contigs_output_dir) {
	INFO("Outputting contigs to " << contigs_output_dir);
	int n = 0;
	make_dir(contigs_output_dir);
	char n_str[20];
	set<ConjugateDeBruijnGraph::EdgeId> edges;
	for (auto it = g.SmartEdgeBegin(); !it.IsEnd(); ++it) {
		if (edges.count(*it) == 0) {
			sprintf(n_str, "%d.fa", n);
			edges.insert(g.conjugate(*it));
			osequencestream oss(contigs_output_dir + n_str);
			oss << g.EdgeNucls(*it);
			n++;
		}
	}DEBUG("SingleFileContigs(Conjugate) written");
}

}
