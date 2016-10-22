﻿using System.IO;
using Simion;
using System.Xml;

namespace Badger.ViewModels
{
    public class ForkedNodeViewModel : ConfigNodeViewModel
    {
        public ConfigNodeViewModel selectedForkValue
        {
            get { return fork.selectedForkValue.forkValue; }
            set
            {
                //we don't need to set it (the actual value will be set within the ForkViewModel class)
                //we just notify that the selection has changed
                NotifyOfPropertyChange(() => selectedForkValue);
            }
        }

        private ForkViewModel m_fork;
        public ForkViewModel fork { get { return m_fork; } set { m_fork = value; } }
        //Constructor used from the experiment editor
        public ForkedNodeViewModel(AppViewModel appViewModel,ConfigNodeViewModel forkedNode)
        {
            m_appViewModel = appViewModel;
            nodeDefinition = forkedNode.nodeDefinition;
            name = forkedNode.name;
            NotifyOfPropertyChange(() => selectedForkValue);
        }
        //Constructor called when loading an experiment config file
        public ForkedNodeViewModel(AppViewModel appViewModel,ConfigNodeViewModel parentNode
            ,XmlNode classDefinition,XmlNode configNode)
        {
            m_appViewModel = appViewModel;
            name = configNode.Attributes[XMLConfig.nameAttribute].Value;
            fork = new ForkViewModel(appViewModel,name,this);
            foreach (XmlNode forkValueConfig in configNode.ChildNodes)
            {
                fork.values.Add(new ForkValueViewModel(appViewModel, classDefinition,parentNode
                    ,fork, forkValueConfig));
            }
            //notify changes
            if (fork.values.Count > 0)
                fork.selectedForkValue = fork.values[0];
            fork.NotifyOfPropertyChange(() => fork.values);
            m_appViewModel.addFork(fork);
        }
        public override bool validate()
        {
            foreach (ForkValueViewModel value in fork.values)
            {
                if (!value.forkValue.validate()) return false;
            }
            return true;
        }

        public override void outputXML(StreamWriter writer,string leftSpace)
        {
            if (m_appViewModel.saveMode == SaveMode.SaveForks)
            {
                writer.WriteLine(leftSpace + "<" + XMLConfig.forkedNodeTag + " " 
                    +XMLConfig.nameAttribute + "=\"" + name.TrimEnd(' ') + "\">");
                fork.outputXML(writer, leftSpace + "  ");
                writer.WriteLine(leftSpace + "</" + XMLConfig.forkedNodeTag + ">");
            }
            else
                selectedForkValue.outputXML(writer, leftSpace);
        }
    }
}