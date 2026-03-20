using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Autodesk.Maya.OpenMaya;
using Autodesk.Maya.MetaData;

namespace metadata
{
	public partial class ChannelForm : Form
	{
		private Associations _metadatas;
		private Channel CurrentlySelectedChannel = null;
		private bool SelectionActive = false;

		public ChannelForm(Associations metadatas)
		{
			_metadatas = metadatas;
			InitializeComponent();
		}

		private void ChannelForm_Load(object sender, EventArgs e)
		{
			try
			{
				SelectionActive = false;

				if (_metadatas != null)
				{
					foreach (var chn in _metadatas)
					{
						Object[] arr = new Object[3];

						arr[0] = chn.name; 
						arr[1] = chn.dataStreamCount;

						dataGridView1.Rows.Add(arr);
					}
				}

				dataGridView1.ClearSelection();
				CurrentlySelectedChannel = null;

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
				if (dataGridView1[0, dataGridView1.SelectedRows[0].Index].Value != null)
				{
					int Steps = dataGridView1.SelectedRows[0].Index;
					foreach (Channel chn in _metadatas)
					{
						if (Steps == 0)
						{
							CurrentlySelectedChannel = chn;
							break;
						}

						Steps--;
					}
				}
			}
		}

		private void ChannelForm_FormClosing(object sender, FormClosingEventArgs e)
		{
			_metadatas = null;
			CurrentlySelectedChannel = null;
		}

		private void dataGridView1_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
		{
			// Examine the currently selected row
			if (CurrentlySelectedChannel != null)
			{
				if (CurrentlySelectedChannel.Count > 0)
				{
					StreamForm w = new StreamForm(CurrentlySelectedChannel);
					w.Text = "Stream(s) available on channel " + CurrentlySelectedChannel.name;
					w.ShowDialog();
				}
			}
		}
	}
}
