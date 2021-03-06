#ifndef INNERVATION_H
#define INNERVATION_H

#include "NetworkStatistic.h"
#include "Histogram.h"
#include "FormulaParser.h"

typedef exprtk::symbol_table<float> symbol_table_t;
typedef exprtk::expression<float> expression_t;
typedef exprtk::parser<float> parser_t;

class SparseVectorCache;

/**
    Represents the standard innervation statistic (as initially available
    in CortexInSilico).
*/
class InnervationStatistic : public NetworkStatistic
{
public:
    /**
        Constructor.
        @param networkProps The model data of the network.
        @param innervation BinSize Bin size of the innervation histogram.
        @param conProbBinSize Bin size of the connectionProbability histogram.
    */
    InnervationStatistic(const NetworkProps& networkProps,
                         const float innervationBinSize = 0.1f,
                         const float connProbBinSize = 0.05f);

    /**
        Constructor.
        @param networkProps The model data of the network.
        @param innervation BinSize Bin size of the innervation histogram.
        @param conProbBinSize Bin size of the connectionProbability histogram.
        @param cache Cache of preloaded innervation values.
    */
    InnervationStatistic(const NetworkProps& networkProps,
                         const SparseVectorCache& cache,
                         const float innervationBinSize = 0.1f,
                         const float connProbBinSize = 0.05f);

    void setExpression(QString expression);

protected:
    /**
        Performs the actual computation based on the specified neurons.
        @param selection The selected neurons.
    */
    void doCalculate(const NeuronSelection& selection) override;

    /**
        Adds the result values to a JSON object
        @param obj: JSON object to which the values are appended
    */
    void doCreateJson(QJsonObject& obj) const override;

    /**
        Writes the result values to file stream (CSV).
        @param out The file stream to which the values are written.
        @param sep The separator between parameter name and value.
    */
    void doCreateCSV(QTextStream& out, const QChar sep) const override;

private:
    Histogram innervationHisto;
    Histogram connProbHisto;

    Statistics innervation;
    Statistics innervationUnique;
    Statistics connProb;
    Statistics connProbUnique;

    Statistics innervationPerPre;
    Statistics innervationPerPreUnique;
    Statistics divergence;
    Statistics divergenceUnique;

    Statistics innervationPerPost;
    Statistics convergence;

    int numPreNeurons;
    int numPostNeurons;
    int numPreNeuronsUnique;
    std::string mExpression;
};

#endif // INNERVATION_H
