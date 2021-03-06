﻿using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using Badger.Data.NeuralNetwork.Parameters;

namespace Badger.Data.NeuralNetwork.Links
{
    [Serializable]
    public class Convolution1DLayer : LinkBase
    {
        private Convolution1DLayer() : base() { }
        public Convolution1DLayer(Chain parent) : base(parent) { }

        public Convolution1DLayer(Chain parent, string name) : base(parent, name)
        {
            Parameters.Add(new IntParameter("Filters", this));
            Parameters.Add(new IntTuple1DParameter("Kernel Size", this));
            Parameters.Add(new IntTuple1DParameter("Stride", this));
            Parameters.Add(new ActivationFunctionParameter("Activation", this));
        }

        public override string TypeName { get { return "1D Convolution Layer"; } }

        public override bool IsInputCompatible
        {
            get
            {
                LinkBase previousLink = GetPreviousLink();
                if (previousLink == null)
                {
                    return false;
                }

                return (previousLink.GetTensorRank() == 2);
            }
        }
        
        public override int? GetTensorRank()
        {
            return 2;
        }
    }
}
