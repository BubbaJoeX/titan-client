using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Autodesk.Maya;
using Autodesk.Maya.OpenMaya;
using Autodesk.Maya.MetaData;

namespace metadata
{
	public partial class MainForm : Form
	{
		private bool SelectionActive = false;
		private MDagPath CurrentlySelectedDP = null;

		public MainForm()
		{
			InitializeComponent();
		}

		private void MainForm_Load(object sender, EventArgs e)
		{
			try
			{
				SelectionActive = false;

				var dag = new MCDag();
				foreach (var dagpath in dag.DagPaths)
				{
					MFnDagNode dagNode = new MFnDagNode(dagpath.node);

					if (dagNode.childCount > 0)
					{
						MObject obj = dagNode.child(0);
						MFnDependencyNode node = new MFnDependencyNode(obj);

						if (node.metadata != null)
						{
							int mdCount = node.metadata.Count;

							Object[] arr = new Object[3];

							arr[0] = dagNode.partialPathName;
							arr[1] = mdCount;
							arr[2] = dagNode.fullPathName;

							dataGridView1.Rows.Add(arr);
						}
					}
				}

				dataGridView1.ClearSelection();
				CurrentlySelectedDP = null;

				SelectionActive = true;
			}

			catch
			{
				Console.WriteLine("Some weird error occured");
			}
		}

		private void dataGridView1_SelectionChanged(object sender, EventArgs e)
		{
			if (SelectionActive && dataGridView1.SelectedRows.Count > 0)
			{
				var v = dataGridView1[2, dataGridView1.SelectedRows[0].Index].Value;

				if (v != null)
				{
					string pathName = v.ToString();

					MDagPath dp = new MDagPath();
					dp.InitFromString(pathName);
					CurrentlySelectedDP = dp;
				}
			}
		}

		private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
		{
			CurrentlySelectedDP = null;
		}

		private void dataGridView1_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
		{
			if (CurrentlySelectedDP != null)
			{
				MFnDagNode dagNode = new MFnDagNode(CurrentlySelectedDP);

				if (dagNode.childCount > 0)
				{
					MObject obj = dagNode.child(0);
					MFnDependencyNode node = new MFnDependencyNode(obj);
					if (node.metadata != null)
					{
						ChannelForm w = new ChannelForm(node.metadata);
						w.Text = "Channel(s) available on object " + CurrentlySelectedDP.partialPathName;
						w.ShowDialog();
					}
				}
			}
		}
	}
}
